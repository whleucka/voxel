#include "world.hpp"
#include "greedy_mesher.hpp"
#include <cmath>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

// std::cout per chunk will hitch. Gate it
static constexpr bool kLogLoads = false;
static constexpr bool kLogUnloads = false;

bool isAABBInFrustum(const glm::vec4 planes[6], const glm::vec3 &min,
                     const glm::vec3 &max) {
  for (int i = 0; i < 6; i++) {
    glm::vec3 positive_vertex = min;
    if (planes[i].x >= 0) {
      positive_vertex.x = max.x;
    }
    if (planes[i].y >= 0) {
      positive_vertex.y = max.y;
    }
    if (planes[i].z >= 0) {
      positive_vertex.z = max.z;
    }

    if (glm::dot(glm::vec3(planes[i]), positive_vertex) + planes[i].w < -0.001f) {
      return false;
    }
  }
  return true;
}

// Promote N ready chunks from worker → main thread world
void World::promotePendingGenerated(int budget) {
  std::deque<GeneratedData> local;

  // Phase 1: drain pending (only pending_mutex)
  {
    std::lock_guard<std::mutex> lk(pending_mutex);
    for (int i = 0; i < budget && !pending_generated.empty(); ++i) {
      local.emplace_back(std::move(pending_generated.front()));
      pending_generated.pop_front();
    }
  }

  if (local.empty())
    return;

  // Phase 2: publish to world / schedule uploads (only chunks_mutex)
  {
    std::lock_guard<std::mutex> lock(chunks_mutex);
    for (auto &item : local) {
      const uint64_t key = item.key;
      loading.erase(key);
      chunks[key] = std::move(item.chunk);
      Chunk &chunk = *chunks[key];
      if (!item.opaque_mesh.vertices.empty()) {
        pending_uploads.push_back(
            {key, &chunk.opaqueMesh, std::move(item.opaque_mesh)});
      }
      if (!item.transparent_mesh.vertices.empty()) {
        pending_uploads.push_back(
            {key, &chunk.transparentMesh, std::move(item.transparent_mesh)});
      }
    }
  }
}

World::World() {
  // Immediately promote a few to avoid blank start
  promotePendingGenerated(64); // generous first tick
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

void World::spiralLoadOrderCircle(int r,
                                  std::vector<std::pair<int, int>> &out) {
  out.clear();
  out.reserve(static_cast<size_t>(3.3 * r * r) + 1);

  auto push_key = [&](int dx, int dz) {
    if (dx * dx + dz * dz <= r * r) {
      out.emplace_back(dx, dz);
    }
  };

  int x = 0, z = 0, dx = 1, dz = 0, step = 1;
  push_key(0, 0);
  while (std::max(std::abs(x), std::abs(z)) <= r) {
    for (int leg = 0; leg < 2; ++leg) {
      for (int s = 0; s < step; ++s) {
        x += dx;
        z += dz;
        if (std::abs(x) <= r && std::abs(z) <= r)
          push_key(x, z);
      }
      int ndx = dz, ndz = -dx;
      dx = ndx;
      dz = ndz;
    }
    ++step;
  }
}

void World::update(const Camera &cam) {
  // Promote finished worker chunks first (so they can render)
  promotePendingGenerated(/*budget=*/8);

  // --- camera chunk coords ---
  const glm::vec3 camera_pos = cam.getPosition();
  const int cam_cx =
      floorDiv(static_cast<int>(std::floor(camera_pos.x)), Chunk::W);
  const int cam_cz =
      floorDiv(static_cast<int>(std::floor(camera_pos.z)), Chunk::L);

  const int r = render_distance;
  const int r2 = r * r;

  // nearest-first using spiral order
  static thread_local std::vector<std::pair<int, int>> order;
  spiralLoadOrderCircle(r, order);

  // Frustum culling for prioritizing loads
  glm::mat4 projection = glm::perspective(
      glm::radians(45.0f), 1920.f / 1080.f, 0.5f, 512.0f);
  glm::mat4 view = cam.getViewMatrix();
  glm::mat4 viewProj = projection * view;
  glm::vec4 planes[6];
  cam.getFrustumPlanes(planes, viewProj);

  std::vector<std::pair<int, int>> priority_loads;
  std::vector<std::pair<int, int>> background_loads;

  for (auto [dx, dz] : order) {
    if (dx * dx + dz * dz > r2)
      continue;
    const int cx = cam_cx + dx;
    const int cz = cam_cz + dz;
    const uint64_t key = makeChunkKey(cx, cz);

    {
      std::lock_guard<std::mutex> lock(chunks_mutex);
      if (chunks.count(key))
        continue; // already present
    }
    if (loading.count(key))
      continue; // already loading

    glm::vec3 min = glm::vec3(cx * Chunk::W, 0, cz * Chunk::L);
    glm::vec3 max = min + glm::vec3(Chunk::W, Chunk::H, Chunk::L);

    if (isAABBInFrustum(planes, min, max)) {
      priority_loads.push_back({cx, cz});
    } else {
      background_loads.push_back({cx, cz});
    }
  }

  // special knobs
  const int load_budget = 6;
  const size_t workers_hint = 8;
  const size_t inflight_cap = workers_hint * 2;

  int loads_left = load_budget;

  // Only try to enqueue if we don't already have a lot in-flight
  if (loading.size() < inflight_cap) {
    // Prioritize frustum-visible chunks
    for (auto [cx, cz] : priority_loads) {
      if (loads_left == 0)
        break;
      loadChunk(cx, cz);
      --loads_left;
    }
    // Then load background chunks
    for (auto [cx, cz] : background_loads) {
      if (loads_left == 0)
        break;
      loadChunk(cx, cz);
      --loads_left;
    }
  }

  // UNLOAD: kick out chunks outside the circle (cap per frame if needed)
  std::vector<uint64_t> toUnload;
  {
    std::lock_guard<std::mutex> lock(chunks_mutex);
    toUnload.reserve(chunks.size());
    for (const auto &kv : chunks) {
      int cx, cz;
      unpackChunkKey(kv.first, cx, cz);
      const int dx = cx - cam_cx;
      const int dz = cz - cam_cz;
      if (dx * dx + dz * dz > r2) {
        toUnload.push_back(kv.first);
        if (kLogUnloads) {
          std::cout << "CONSIDERING UNLOAD CHUNK " << cx << "," << cz
                    << " (dist^2: " << dx * dx + dz * dz << ", r2: " << r2
                    << ")\n";
        }
      }
    }
  }
  // optional cap: size_t unload_budget = 16;
  for (auto key : toUnload) {
    int cx, cz;
    unpackChunkKey(key, cx, cz);
    unloadChunk(cx, cz);
    if (kLogUnloads) {
      std::cout << "UNLOADED CHUNK " << cx << "," << cz << "\n";
    }
    // if (--unload_budget == 0) break;
  }
}

std::vector<Chunk *> World::getVisibleChunks(const Camera &cam, int width,
                                             int height) {
  std::lock_guard<std::mutex> lock(chunks_mutex);

  // Frustum culling
  glm::mat4 projection = glm::perspective(
      glm::radians(45.0f), (float)width / (float)height, 0.5f, 512.0f);
  glm::mat4 view = cam.getViewMatrix();
  glm::mat4 viewProj = projection * view;
  glm::vec4 planes[6];
  cam.getFrustumPlanes(planes, viewProj);

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
    if (dx * dx + dz * dz > r2) {
      continue;
    }

    glm::vec3 min = glm::vec3(cx * Chunk::W, 0, cz * Chunk::L);
    glm::vec3 max = min + glm::vec3(Chunk::W, Chunk::H, Chunk::L);

    if (isAABBInFrustum(planes, min, max)) {
      result.push_back(uptr.get());
    }
  }
  return result;
}

BlockType World::getBlock(int x, int y, int z) const {
  std::lock_guard<std::mutex> lock(chunks_mutex);
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
  std::lock_guard<std::mutex> lock(chunks_mutex);
  int cx = x / Chunk::W;
  int cz = z / Chunk::L;
  uint64_t key = makeChunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    it->second->setBlock(x % Chunk::W, y, z % Chunk::L, type);
  }
}

void World::loadChunk(int cx, int cz) {
  const uint64_t key = makeChunkKey(cx, cz);

  {
    std::lock_guard<std::mutex> lock(chunks_mutex);
    if (chunks.find(key) != chunks.end())
      return; // already present
  }
  if (loading.find(key) != loading.end())
    return; // already queued

  loading.insert(key);
  if (kLogLoads)
    std::cout << "ENQUEUE CHUNK" << cx << "," << cz << "\n";

  // Worker job: build the chunk data OFF the main thread
  thread_pool.enqueue([this, cx, cz, key]() {
    auto up = std::make_unique<Chunk>(cx, cz);
    up->generateChunk();

    auto sample = [this, &up](int gx, int gy, int gz) -> BlockType {
      int local_x = gx - up->world_x * Chunk::W;
      int local_y = gy;
      int local_z = gz - up->world_z * Chunk::L;
      if (local_x >= 0 && local_x < Chunk::W && local_z >= 0 &&
          local_z < Chunk::L) {
        return up->getBlock(local_x, local_y, local_z);
      }
      return this->getBlock(gx, gy, gz);
    };

    auto [opaque_mesh, transparent_mesh] = GreedyMesher::build_cpu(*up, sample);

    // Hand it back to the main thread queue
    {
      std::lock_guard<std::mutex> lk(pending_mutex);
      pending_generated.emplace_back(
          GeneratedData{key, std::move(up), std::move(opaque_mesh),
                        std::move(transparent_mesh)});
    }
  });
}

void World::unloadChunk(int cx, int cz) {
  const uint64_t key = makeChunkKey(cx, cz);

  pending_uploads.erase(std::remove_if(pending_uploads.begin(),
                                       pending_uploads.end(),
                                       [key](const PendingUpload &upload) {
                                         return upload.chunk_key == key;
                                       }),
                        pending_uploads.end());

  loading.erase(key);
  if (kLogUnloads)
    std::cout << "UNLOAD CHUNK " << cx << "," << cz << "\n";
  std::lock_guard<std::mutex> lock(chunks_mutex);
  chunks.erase(key);
}

void World::processUploads() {
  if (pending_uploads.empty()) {
    return;
  }

  int uploads_processed = 0;
  const int max_uploads_per_frame = 1;

  auto it = pending_uploads.begin();
  while (it != pending_uploads.end() &&
         uploads_processed < max_uploads_per_frame) {
    if (it->targetMesh) {
      it->targetMesh->upload(it->mesh.vertices, it->mesh.indices);
    }
    it = pending_uploads.erase(it);
    uploads_processed++;
  }
}

void World::generateChunkData(int cx, int cz) {
  const uint64_t key = makeChunkKey(cx, cz);
  {
    std::lock_guard<std::mutex> lock(chunks_mutex);
    if (chunks.find(key) != chunks.end()) {
      return;
    }
  }
  auto chunk = std::make_unique<Chunk>(cx, cz);
  chunk->generateChunk();
  {
    std::lock_guard<std::mutex> lock(chunks_mutex);
    chunks[key] = std::move(chunk);
  }
}

void World::generateChunkMesh(int cx, int cz) {
  const uint64_t key = makeChunkKey(cx, cz);
  Chunk *chunk;
  {
    std::lock_guard<std::mutex> lock(chunks_mutex);
    auto it = chunks.find(key);
    if (it == chunks.end()) {
      return;
    }
    chunk = it->second.get();
  }

  auto sample = [this, chunk](int gx, int gy, int gz) -> BlockType {
    int local_x = gx - chunk->world_x * Chunk::W;
    int local_y = gy;
    int local_z = gz - chunk->world_z * Chunk::L;
    if (local_x >= 0 && local_x < Chunk::W && local_z >= 0 &&
        local_z < Chunk::L) {
      return chunk->getBlock(local_x, local_y, local_z);
    }
    return this->getBlock(gx, gy, gz);
  };
  auto [opaque_mesh, transparent_mesh] =
      GreedyMesher::build_cpu(*chunk, sample);

  {
    std::lock_guard<std::mutex> lock(chunks_mutex);
    if (!opaque_mesh.vertices.empty()) {
      pending_uploads.push_back(
          {key, &chunk->opaqueMesh, std::move(opaque_mesh)});
    }
    if (!transparent_mesh.vertices.empty()) {
      pending_uploads.push_back(
          {key, &chunk->transparentMesh, std::move(transparent_mesh)});
    }
  }
}

void World::processAllUploads() {
  auto it = pending_uploads.begin();
  while (it != pending_uploads.end()) {
    if (it->targetMesh) {
      it->targetMesh->upload(it->mesh.vertices, it->mesh.indices);
    }
    it = pending_uploads.erase(it);
  }
}

int World::getHighestBlock(int x, int z) {
  for (int y = Chunk::H - 1; y >= 0; y--) {
    if (getBlock(x, y, z) != BlockType::AIR) {
      return y + 1;
    }
  }
  return 0;
}
