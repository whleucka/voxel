#include "world.hpp"
#include "biome_manager.hpp"
#include "greedy_mesher.hpp"
#include "utils.hpp"
#include <cmath>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <iostream>

// std::cout per chunk will hitch. Gate it
static constexpr bool kLogLoads = false;
static constexpr bool kLogUnloads = false;

bool isAABBInFrustum(const glm::vec4 planes[6], const glm::vec3 &min,
                     const glm::vec3 &max) {
  for (int i = 0; i < 6; i++) {
    glm::vec3 positive_vertex = min;
    if (planes[i].x >= 0)
      positive_vertex.x = max.x;
    if (planes[i].y >= 0)
      positive_vertex.y = max.y;
    if (planes[i].z >= 0)
      positive_vertex.z = max.z;

    if (glm::dot(glm::vec3(planes[i]), positive_vertex) + planes[i].w < 0.0f) {
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
      glm::radians(45.0f), 1920.f / 1080.f, 0.1f, 512.0f);
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
      const int render_distance_buffer = 2;
      if (dx * dx + dz * dz > r2 + render_distance_buffer) {
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
      glm::radians(45.0f), (float)width / (float)height, 0.1f, 512.0f);
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
  if (y < 0 || y >= Chunk::H) {
    return;
  }

  const int cx = floorDiv(x, Chunk::W);
  const int cz = floorDiv(z, Chunk::L);
  const uint64_t key = makeChunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    const int lx = floorMod(x, Chunk::W);
    const int lz = floorMod(z, Chunk::L);
    it->second->setBlock(lx, y, lz, type);
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
    
    BiomeType biomeType = BiomeManager::getBiomeForChunk(cx, cz);
    std::unique_ptr<Biome> biome = BiomeManager::createBiome(biomeType);
    biome->generateTerrain(*up);
    biome->spawnDecorations(*up);

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

void World::processAllUploads() {
  auto it = pending_uploads.begin();
  while (it != pending_uploads.end()) {
    if (it->targetMesh) {
      it->targetMesh->upload(it->mesh.vertices, it->mesh.indices);
    }
    it = pending_uploads.erase(it);
  }
}

std::optional<std::tuple<glm::ivec3, glm::ivec3>>
World::raycast(const glm::vec3 &start, const glm::vec3 &direction,
               float max_dist) const {
  glm::ivec3 block_pos = glm::floor(start);
  glm::vec3 ray_unit_step =
      glm::vec3(glm::abs(1.0f / direction.x), glm::abs(1.0f / direction.y),
                glm::abs(1.0f / direction.z));

  glm::ivec3 step;
  glm::vec3 ray_len;

  if (direction.x < 0) {
    step.x = -1;
    ray_len.x = (start.x - float(block_pos.x)) * ray_unit_step.x;
  } else {
    step.x = 1;
    ray_len.x = (float(block_pos.x + 1) - start.x) * ray_unit_step.x;
  }
  if (direction.y < 0) {
    step.y = -1;
    ray_len.y = (start.y - float(block_pos.y)) * ray_unit_step.y;
  } else {
    step.y = 1;
    ray_len.y = (float(block_pos.y + 1) - start.y) * ray_unit_step.y;
  }
  if (direction.z < 0) {
    step.z = -1;
    ray_len.z = (start.z - float(block_pos.z)) * ray_unit_step.z;
  } else {
    step.z = 1;
    ray_len.z = (float(block_pos.z + 1) - start.z) * ray_unit_step.z;
  }

  glm::ivec3 normal(0);
  while (glm::distance(start, glm::vec3(block_pos)) < max_dist) {
    if (getBlock(block_pos.x, block_pos.y, block_pos.z) != BlockType::AIR) {
      return std::make_tuple(block_pos, normal);
    }

    if (ray_len.x < ray_len.y && ray_len.x < ray_len.z) {
      block_pos.x += step.x;
      ray_len.x += ray_unit_step.x;
      normal = glm::ivec3(-step.x, 0, 0);
    } else if (ray_len.y < ray_len.z) {
      block_pos.y += step.y;
      ray_len.y += ray_unit_step.y;
      normal = glm::ivec3(0, -step.y, 0);
    } else {
      block_pos.z += step.z;
      ray_len.z += ray_unit_step.z;
      normal = glm::ivec3(0, 0, -step.z);
    }
  }

  return std::nullopt;
}

void World::remeshChunk(int cx, int cz) {
  const uint64_t key = makeChunkKey(cx, cz);

  Chunk* chunk_ptr = nullptr;
  {
      std::lock_guard<std::mutex> lock(chunks_mutex);
      auto it = chunks.find(key);
      if (it == chunks.end()) {
          return;
      }
      chunk_ptr = it->second.get();
  }


  thread_pool.enqueue([this, chunk_ptr, key]() {
    auto start = std::chrono::high_resolution_clock::now();
    auto sample = [this, chunk_ptr](int gx, int gy, int gz) -> BlockType {
      int local_x = gx - chunk_ptr->world_x * Chunk::W;
      int local_y = gy;
      int local_z = gz - chunk_ptr->world_z * Chunk::L;
      if (local_x >= 0 && local_x < Chunk::W && local_z >= 0 &&
          local_z < Chunk::L) {
        return chunk_ptr->getBlock(local_x, local_y, local_z);
      }
      return this->getBlock(gx, gy, gz);
    };

    auto [opaque_mesh, transparent_mesh] = GreedyMesher::build_cpu(*chunk_ptr, sample);

    // Hand it back to the main thread queue
    {
      std::lock_guard<std::mutex> lk(pending_mutex);
      pending_uploads.push_back({key, &chunk_ptr->opaqueMesh, std::move(opaque_mesh)});
      pending_uploads.push_back({key, &chunk_ptr->transparentMesh, std::move(transparent_mesh)});
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "remeshChunk took " << duration.count() << "ms" << std::endl;
  });
}