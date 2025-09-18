#include "world.hpp"
#include "greedy_mesher.hpp"
#include <cmath>
#include <iostream>

// std::cout per chunk will hitch. Gate it
static constexpr bool kLogLoads = false;
static constexpr bool kLogUnloads = false;

static inline void spiralOrder(int r, std::vector<std::pair<int, int>> &out) {
  out.clear();
  out.reserve((2 * r + 1) * (2 * r + 1));

  int x = 0, z = 0;
  int dx = 1, dz = 0; // start moving +x
  int step = 1;

  out.emplace_back(0, 0);
  while (std::max(std::abs(x), std::abs(z)) < r) {
    // two legs per ring
    for (int leg = 0; leg < 2; ++leg) {
      for (int s = 0; s < step; ++s) {
        x += dx;
        z += dz;
        if (std::abs(x) <= r && std::abs(z) <= r)
          out.emplace_back(x, z);
      }
      // right turn: (dx, dz) = (dz, -dx)
      int ndx = dz, ndz = -dx;
      dx = ndx;
      dz = ndz;
    }
    ++step;
  }
}

// Promote N ready chunks from worker → main thread world
void World::promotePendingGenerated(int budget) {
  std::deque<GeneratedData> local;
  {
    std::lock_guard<std::mutex> lk(pending_mutex);
    // Move up to `budget` to a local queue
    for (int i = 0; i < budget && !pending_generated.empty(); ++i) {
      local.emplace_back(std::move(pending_generated.front()));
      pending_generated.pop_front();
    }
  }
  // No locks from here down
  for (auto &item : local) {
    const uint64_t key = item.key;
    int cx = (int)(int32_t)(key >> 32);
    int cz = (int)(int32_t)(key & 0xffffffffu);
    loading.erase(key);
    chunks[key] = std::move(item.chunk);

    Chunk& chunk = *chunks[key];

    if (!item.opaque_mesh.vertices.empty()) {
        pending_uploads.push_back({key, &chunk.opaqueMesh, std::move(item.opaque_mesh)});
    }
    if (!item.transparent_mesh.vertices.empty()) {
        pending_uploads.push_back({key, &chunk.transparentMesh, std::move(item.transparent_mesh)});
    }

    auto mark_if_present = [&](int nx, int nz) {
      if (chunks.count(makeChunkKey(nx, nz)))
        dirty.insert(makeChunkKey(nx, nz));
    };
    mark_if_present(cx + 1, cz);
    mark_if_present(cx - 1, cz);
    mark_if_present(cx, cz + 1);
    mark_if_present(cx, cz - 1);
  }
}

World::World() {
  // Pre-load around origin synchronously so you see something right away
  for (int dx = -render_distance; dx <= render_distance; ++dx) {
    for (int dz = -render_distance; dz <= render_distance; ++dz) {
      if (dx * dx + dz * dz > render_distance * render_distance)
        continue;
      loadChunk(dx, dz); // This will enqueue now; for "instant", you can
                         // special-case to sync generate here.
    }
  }
  // Immediately promote a few to avoid blank start
  promotePendingGenerated(64); // generous first tick
  generateMeshes();
}

World::~World() = default;

static inline int floorDiv(int a, int b) {
  int q = a / b, r = a % b;
  if (r != 0 && ((r < 0) != (b < 0)))
    --q;
  return q;
}
static inline int floorMod(int a, int b) {
  int r = a % b;
  return (r < 0) ? r + b : r;
}

static inline void unpackChunkKey(uint64_t key, int &cx, int &cz) {
  cx = static_cast<int>(static_cast<int32_t>(key >> 32));
  cz = static_cast<int>(static_cast<int32_t>(key & 0xffffffffu));
}

void World::update(const glm::vec3 &camera_pos) {
  // --- 0) Promote finished worker chunks first (so they can render) ---
  promotePendingGenerated(/*budget=*/8);

  // --- camera chunk coords ---
  const int cam_cx =
      floorDiv(static_cast<int>(std::floor(camera_pos.x)), Chunk::W);
  const int cam_cz =
      floorDiv(static_cast<int>(std::floor(camera_pos.z)), Chunk::L);

  const int r = render_distance;
  const int r2 = r * r;

  // --- 1) LOAD: nearest-first using spiral order ---
  static thread_local std::vector<std::pair<int, int>> order;
  spiralOrder(r, order);

  // knobs
  const int load_budget = 6;
  const size_t workers_hint = 8;
  const size_t inflight_cap = workers_hint * 2;

  int loads_left = load_budget;

  // Only try to enqueue if we don't already have a lot in-flight
  if (loading.size() < inflight_cap) {
    for (auto [dx, dz] : order) {
      if (dx * dx + dz * dz > r2)
        continue; // optional circle mask
      const int cx = cam_cx + dx;
      const int cz = cam_cz + dz;
      const uint64_t key = makeChunkKey(cx, cz);

      if (chunks.find(key) == chunks.end() &&
          loading.find(key) == loading.end()) {
        loadChunk(cx, cz); // enqueue to thread pool
        if (--loads_left == 0)
          break;
        if (loading.size() >= inflight_cap)
          break; // back off if queue fills
      }
    }
  }

  // --- 2) UNLOAD: kick out chunks outside the circle (cap per frame if needed)
  // ---
  std::vector<uint64_t> toUnload;
  toUnload.reserve(chunks.size());
  for (const auto &kv : chunks) {
    int cx, cz;
    unpackChunkKey(kv.first, cx, cz);
    const int dx = cx - cam_cx, dz = cz - cam_cz;
    if (dx * dx + dz * dz > r2)
      toUnload.push_back(kv.first);
  }
  // optional cap: size_t unload_budget = 16;
  for (auto key : toUnload) {
    unloadChunk(static_cast<int>(static_cast<int32_t>(key >> 32)),
                static_cast<int>(static_cast<int32_t>(key & 0xffffffffu)));
    // if (--unload_budget == 0) break;
  }

  // --- 3) Mesh a bit each frame on main thread (your function already
  // time/qty-budgets) ---
  generateMeshes();
}

std::vector<Chunk *> World::getVisibleChunks(const Camera &cam) {
  // unchanged (distance filter)
  const glm::vec3 p = cam.getPosition();
  const int cam_cx = floorDiv(static_cast<int>(std::floor(p.x)), Chunk::W);
  const int cam_cz = floorDiv(static_cast<int>(std::floor(p.z)), Chunk::L);
  const int r2 = render_distance * render_distance;

  std::vector<Chunk *> result;
  result.reserve(chunks.size());
  for (auto &[key, uptr] : chunks) {
    int cx, cz;
    unpackChunkKey(key, cx, cz);
    const int dx = cx - cam_cx, dz = cz - cam_cz;
    if (dx * dx + dz * dz <= r2)
      result.push_back(uptr.get());
  }
  return result;
}

BlockType World::getBlock(int x, int y, int z) const {
  if (y < 0 || y >= Chunk::H)
    return BlockType::AIR;

  const int cx = floorDiv(x, Chunk::W);
  const int cz = floorDiv(z, Chunk::L);
  const uint64_t key = makeChunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it == chunks.end())
    return BlockType::AIR;

  const int lx = floorMod(x, Chunk::W);
  const int lz = floorMod(z, Chunk::L);
  return it->second->getBlock(lx, y, lz);
}

void World::setBlock(int x, int y, int z, BlockType type) {
  int cx = x / Chunk::W;
  int cz = z / Chunk::L;
  uint64_t key = makeChunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    it->second->setBlock(x % Chunk::W, y, z % Chunk::L, type);
  }
}

void World::generateMeshes() {
  if (dirty.empty())
    return;

  auto sample = [&](int gx, int gy, int gz) -> BlockType {
    return getBlock(gx, gy, gz);
  };

  using clock = std::chrono::steady_clock;
  const auto t0 = clock::now();
  const auto budget = std::chrono::milliseconds(2); // ~2 ms per frame

  // Pull keys into a small vector to avoid iterator invalidation fiddling
  std::vector<uint64_t> toBuild;
  toBuild.reserve(8);
  for (auto key : dirty) {
    toBuild.push_back(key);
    if (toBuild.size() >= 8)
      break;
  }

  for (auto key : toBuild) {
    auto it = chunks.find(key);
    if (it == chunks.end()) {
      dirty.erase(key);
      continue;
    }
    Chunk &c = *it->second;
    auto [opaque_mesh, transparent_mesh] = GreedyMesher::build_cpu(c, sample);
    if (!opaque_mesh.vertices.empty()) {
        pending_uploads.push_back({key, &c.opaqueMesh, std::move(opaque_mesh)});
    }
    if (!transparent_mesh.vertices.empty()) {
        pending_uploads.push_back({key, &c.transparentMesh, std::move(transparent_mesh)});
    }
    dirty.erase(key);

    if (clock::now() - t0 >= budget)
      break;
  }
}

void World::loadChunk(int cx, int cz) {
  const uint64_t key = makeChunkKey(cx, cz);

  if (chunks.find(key) != chunks.end())
    return; // already present
  if (loading.find(key) != loading.end())
    return; // already queued

  loading.insert(key);
  if (kLogLoads)
    std::cout << "ENQUEUE " << cx << "," << cz << "\n";

  // Worker job: build the chunk data OFF the main thread
  thread_pool.enqueue([this, cx, cz, key]() {
    auto up = std::make_unique<Chunk>(cx, cz);
    up->generateChunk();

    auto sample = [&](int gx, int gy, int gz) -> BlockType {
        int local_x = gx - up->world_x * Chunk::W;
        int local_y = gy;
        int local_z = gz - up->world_z * Chunk::L;
        return up->getBlock(local_x, local_y, local_z);
    };

    auto [opaque_mesh, transparent_mesh] = GreedyMesher::build_cpu(*up, sample);

    // Hand it back to the main thread queue
    {
      std::lock_guard<std::mutex> lk(pending_mutex);
      pending_generated.emplace_back(GeneratedData{key, std::move(up), std::move(opaque_mesh), std::move(transparent_mesh)});
    }
  });
}

void World::unloadChunk(int cx, int cz) {
  const uint64_t key = makeChunkKey(cx, cz);

  pending_uploads.erase(
    std::remove_if(pending_uploads.begin(), pending_uploads.end(),
                   [key](const PendingUpload& upload) {
                     return upload.chunk_key == key;
                   }),
    pending_uploads.end());

  // If it’s still in-flight, just forget about it (when it returns, we won't
  // insert it if you add a check)
  loading.erase(key);
  if (kLogUnloads)
    std::cout << "UNLOAD " << cx << "," << cz << "\n";
  chunks.erase(key);
  dirty.erase(key);
}


void World::processUploads() {
  if (pending_uploads.empty()) {
    return;
  }

  int uploads_processed = 0;
  const int max_uploads_per_frame = 2;

  auto it = pending_uploads.begin();
  while (it != pending_uploads.end() && uploads_processed < max_uploads_per_frame) {
    if (it->targetMesh) {
      it->targetMesh->upload(it->mesh.vertices, it->mesh.indices);
    }
    it = pending_uploads.erase(it);
    uploads_processed++;
  }
}
