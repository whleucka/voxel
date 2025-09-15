#include "world.hpp"
#include "aabb.hpp"
#include "camera.hpp"
#include "coord.hpp"
#include "render_ctx.hpp"
#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

std::vector<ChunkKey> getChunkLoadOrder(int camChunkX, int camChunkZ,
                                        int radius) {
  std::vector<ChunkKey> result;

  for (int dx = -radius; dx <= radius; dx++) {
    for (int dz = -radius; dz <= radius; dz++) {
      // Circle radius (optional). If you want square area, drop this check.
      if (dx * dx + dz * dz <= radius * radius) {
        // (π * r²) render_distance = 18
        // π * 18² is approx 1,017 chunks
        result.push_back({camChunkX + dx, camChunkZ + dz});
      }
    }
  }

  // Sort by squared distance (cheaper than sqrt)
  std::sort(result.begin(), result.end(),
            [&](const ChunkKey &a, const ChunkKey &b) {
              int dax = a.x - camChunkX;
              int daz = a.z - camChunkZ;
              int dbx = b.x - camChunkX;
              int dbz = b.z - camChunkZ;
              return (dax * dax + daz * daz) < (dbx * dbx + dbz * dbz);
            });

  return result;
}

inline int worldToChunkCoord(int pos, int chunkSize) {
  return (pos >= 0) ? (pos / chunkSize) : ((pos + 1) / chunkSize - 1);
}

World::World(Texture &a) : block_atlas(a) { startThreads(); }

World::~World() { stopThreads(); }

void World::loadChunk(int x, int z) {
  ChunkKey key{x, z};

  // Already active
  if (chunks.find(key) != chunks.end())
    return;

  // Reactivate from cache
  auto it = cache.find(key);
  if (it != cache.end()) {
    chunks[key] = it->second;
    cache.erase(it);
    return;
  }

  // Enqueue for threaded generation
  {
    std::lock_guard<std::mutex> lock(_load_q_mutex);
    if (_loading_q.count(key) || _loading_q.size() > max_cache)
      return;
    _load_q.push(key);
    _loading_q.insert(key);
    std::cout << "LOAD CHUNK (" << key.x << ", " << key.z << ")" << std::endl;
  }
  _load_q_cv.notify_one();
}

void World::unloadChunk(const ChunkKey &key) {

  auto it = chunks.find(key);
  if (it != chunks.end()) {
    std::cout << "UNLOAD CHUNK (" << key.x << ", " << key.z << ")" << std::endl;
    // Move chunk into cache
    if (cache.size() < max_cache) {
      cache[key] = it->second;
    } else {
      delete it->second;
    }
    chunks.erase(it);
  }
}

int World::getChunkCount() const { return chunks.size(); }

float World::getMaxChunks() const {
  // Approximately
  return round(glm::pi<float>() * pow(render_distance, 2));
}

Chunk *World::getChunk(int chunk_x, int chunk_z) {
  ChunkKey key{chunk_x, chunk_z};
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    return it->second; // loaded
  }
  return nullptr; // not loaded
}

void World::update(glm::vec3 camera_pos) {
  int cam_cx = worldToChunkCoord(static_cast<int>(camera_pos.x), chunk_width);
  int cam_cz = worldToChunkCoord(static_cast<int>(camera_pos.z), chunk_length);

  auto order = getChunkLoadOrder(cam_cx, cam_cz, render_distance);

  int chunks_loaded_this_frame = 0;
  for (auto &key : order) {
    if (chunks.find(key) == chunks.end()) {
      loadChunk(key.x, key.z);
      if (++chunks_loaded_this_frame >= max_chunks_per_frame)
        break;
    }
  }

  // Unload chunks that are too far away
  std::vector<ChunkKey> toUnload;
  for (auto &[key, chunk] : chunks) {
    int distX = key.x - cam_cx;
    int distZ = key.z - cam_cz;
    if (distX * distX + distZ * distZ > render_distance * render_distance) {
      toUnload.push_back(key);
    }
  }

  for (auto &key : toUnload) {
    unloadChunk(key);
  }

  // Process generated chunks
  {
    std::lock_guard<std::mutex> lock(_generated_q_mutex);
    while (!_generated_q.empty()) {
      Chunk *chunk = _generated_q.front();
      _generated_q.pop();
      chunk->opaqueMesh.setupMesh();
      chunk->transparentMesh.setupMesh();
      chunks[chunk->getChunkKey()] = chunk;
      _loading_q.erase(chunk->getChunkKey());
    }
  }
}

void World::drawOpaque(renderCtx &ctx) {
  if (chunks.empty())
    return;

  glUseProgram(ctx.block_shader.ID);

  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "view"), 1,
                     GL_FALSE, glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(ctx.proj));

  // Get frustum planes from the camera
  glm::vec4 frustumPlanes[6];
  ctx.camera.getFrustumPlanes(frustumPlanes, ctx.proj * ctx.view);

  for (auto &[key, chunk] : chunks) {
    // Perform frustum culling
    if (!chunk->m_aabb.intersectsFrustum(frustumPlanes)) {
      continue; // Skip rendering this chunk if it's outside the frustum
    }

    glm::mat4 model =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(key.x * chunk_width, 0, key.z * chunk_length));
    glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "model"), 1,
                       GL_FALSE, glm::value_ptr(model));
    chunk->drawOpaque(ctx.block_shader); // Fixed: call drawOpaque
  }
}

void World::drawTransparent(renderCtx &ctx) {
  if (chunks.empty())
    return;

  glUseProgram(ctx.block_shader.ID);

  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "view"), 1,
                     GL_FALSE, glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(ctx.proj));

  // Get frustum planes from the camera
  glm::vec4 frustumPlanes[6];
  ctx.camera.getFrustumPlanes(frustumPlanes, ctx.proj * ctx.view);

  // Sort chunks by distance from camera for proper transparent rendering
  std::vector<std::pair<float, Chunk *>> sortedChunks;
  for (auto &[key, chunk] : chunks) {
    glm::vec3 chunkCenter = glm::vec3(key.x * chunk_width + chunk_width / 2.0f,
                                      chunk_height / 2.0f,
                                      key.z * chunk_length + chunk_length / 2.0f);
    float distance = glm::length(ctx.camera.getPos() - chunkCenter);
    sortedChunks.push_back({distance, chunk});
  }
  std::sort(sortedChunks.rbegin(), sortedChunks.rend()); // Sort back to front

  for (auto &[distance, chunk] : sortedChunks) {
    // Perform frustum culling
    if (!chunk->m_aabb.intersectsFrustum(frustumPlanes)) {
      continue; // Skip rendering this chunk if it's outside the frustum
    }

    glm::mat4 model =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(chunk->getChunkKey().x * chunk_width, 0, chunk->getChunkKey().z * chunk_length)); // Fixed: use chunk->getChunkKey()
    glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "model"), 1,
                       GL_FALSE, glm::value_ptr(model));
    chunk->drawTransparent(ctx.block_shader);
  }
}

BlockType World::getBlock(int x, int y, int z) {
  const int cx = worldToChunk(x, chunk_width);
  const int cz = worldToChunk(z, chunk_length);

  auto it = chunks.find(ChunkKey{cx, cz});
  if (it == chunks.end()) {
    // If outside vertical bounds, just return AIR so no walls appear
    if (y < 0 || y >= chunk_height) return BlockType::AIR;

    // Otherwise, truly unknown (neighbor not loaded yet)
    return BlockType::UNKNOWN;
  }

  const int lx = worldToLocal(x, chunk_width);
  const int lz = worldToLocal(z, chunk_length);

  return it->second->getBlock(lx, y, lz);
}

// Thread stuff
void World::startThreads() {
  const unsigned int num_threads = std::thread::hardware_concurrency();
  for (unsigned int i = 0; i < num_threads; ++i) {
    _threads.emplace_back(&World::threadLoop, this);
  }
}

void World::stopThreads() {
  {
    std::lock_guard<std::mutex> lock(_load_q_mutex);
    _should_stop = true;
  }
  _load_q_cv.notify_all();
  for (auto &t : _threads) {
    t.join();
  }
}

void World::threadLoop() {
  while (true) {
    ChunkKey key;
    {
      std::unique_lock<std::mutex> lock(_load_q_mutex);
      _load_q_cv.wait(lock,
                      [this] { return _should_stop || !_load_q.empty(); });
      if (_should_stop) {
        return;
      }
      key = _load_q.front();
      _load_q.pop();
    }

    try {
      // Fresh generate chunk
      Chunk *chunk = new Chunk(chunk_width, chunk_length, chunk_height, key.x,
                               key.z, this);
      chunk->generateMesh(block_atlas);

      {
        std::lock_guard<std::mutex> lock(_generated_q_mutex);
        _generated_q.push(chunk);
      }
    } catch (const std::bad_alloc &e) {
      std::cerr << "Failed to allocate memory for chunk: " << e.what()
                << std::endl;
      _loading_q.erase(key);
    }
  }
}

void World::removeBlock(int x, int y, int z) {
  const int cx = worldToChunk(x, chunk_width);
  const int cz = worldToChunk(z, chunk_length);

  Chunk *chunk = getChunk(cx, cz);
  if (!chunk) {
    return; // chunk not loaded
  }

  const int lx = worldToLocal(x, chunk_width);
  const int lz = worldToLocal(z, chunk_length);

  // Removing a block set's the type to AIR
  chunk->setBlock(lx, y, lz, BlockType::AIR);
  chunk->generateMesh(block_atlas);
  chunk->opaqueMesh.setupMesh();
  chunk->transparentMesh.setupMesh();

  // Check and update neighboring chunks if the block was on a border
  if (lx == 0) {
    Chunk *neighbor = getChunk(cx - 1, cz);
    if (neighbor) {
      neighbor->generateMesh(block_atlas);
      neighbor->opaqueMesh.setupMesh();
      neighbor->transparentMesh.setupMesh();
    }
  } else if (lx == chunk_width - 1) {
    Chunk *neighbor = getChunk(cx + 1, cz);
    if (neighbor) {
      neighbor->generateMesh(block_atlas);
      neighbor->opaqueMesh.setupMesh();
      neighbor->transparentMesh.setupMesh();
    }
  }

  if (lz == 0) {
    Chunk *neighbor = getChunk(cx, cz - 1);
    if (neighbor) {
      neighbor->generateMesh(block_atlas);
      neighbor->opaqueMesh.setupMesh();
      neighbor->transparentMesh.setupMesh();
    }
  } else if (lz == chunk_length - 1) {
    Chunk *neighbor = getChunk(cx, cz + 1);
    if (neighbor) {
      neighbor->generateMesh(block_atlas);
      neighbor->opaqueMesh.setupMesh();
      neighbor->transparentMesh.setupMesh();
    }
  }
}

void World::addBlock(int x, int y, int z, BlockType type) {
  const int cx = worldToChunk(x, chunk_width);
  const int cz = worldToChunk(z, chunk_length);

  Chunk *chunk = getChunk(cx, cz);
  if (!chunk) {
    return; // chunk not loaded
  }

  const int lx = worldToLocal(x, chunk_width);
  const int lz = worldToLocal(z, chunk_length);

  chunk->setBlock(lx, y, lz, type);
  chunk->generateMesh(block_atlas);
  chunk->opaqueMesh.setupMesh();
  chunk->transparentMesh.setupMesh();

  // Check and update neighboring chunks if the block was on a border
  if (lx == 0) {
    Chunk *neighbor = getChunk(cx - 1, cz);
    if (neighbor) {
      neighbor->opaqueMesh.setupMesh();
      neighbor->transparentMesh.setupMesh();
    }
  } else if (lx == chunk_width - 1) {
    Chunk *neighbor = getChunk(cx + 1, cz);
    if (neighbor) {
      neighbor->generateMesh(block_atlas);
      neighbor->opaqueMesh.setupMesh();
      neighbor->transparentMesh.setupMesh();
    }
  }

  if (lz == 0) {
    Chunk *neighbor = getChunk(cx, cz - 1);
    if (neighbor) {
      neighbor->generateMesh(block_atlas);
      neighbor->opaqueMesh.setupMesh();
      neighbor->transparentMesh.setupMesh();
    }
  } else if (lz == chunk_length - 1) {
    Chunk *neighbor = getChunk(cx, cz + 1);
    if (neighbor) {
      neighbor->generateMesh(block_atlas);
      neighbor->opaqueMesh.setupMesh();
      neighbor->transparentMesh.setupMesh();
    }
  }
}

// Amanatides & Woo's voxel traversal (fixed)
bool World::raycast(const glm::vec3 &start, const glm::vec3 &dir,
                    float max_dist, glm::ivec3 &block_pos,
                    glm::ivec3 &face_normal) {
  // Normalize ray direction
  glm::vec3 ray_dir = glm::normalize(dir);

  // Offset start a tiny bit so we don’t immediately collide with the voxel
  // we’re inside
  glm::vec3 pos = start + ray_dir * 0.0001f;

  // Current voxel coordinates
  glm::ivec3 voxel = glm::floor(pos);

  // Step direction per axis
  glm::ivec3 step((ray_dir.x > 0) ? 1 : -1, (ray_dir.y > 0) ? 1 : -1,
                  (ray_dir.z > 0) ? 1 : -1);

  // Compute initial tMax (distance to the first voxel boundary)
  glm::vec3 tMax;
  auto intBound = [](float s, float ds, int step) -> float {
    if (ds == 0.0f)
      return std::numeric_limits<float>::infinity();
    float nextBoundary =
        (step > 0) ? std::floor(s + 1.0f) : std::ceil(s - 1.0f);
    return (nextBoundary - s) / ds;
  };

  tMax.x = intBound(pos.x, ray_dir.x, step.x);
  tMax.y = intBound(pos.y, ray_dir.y, step.y);
  tMax.z = intBound(pos.z, ray_dir.z, step.z);

  // Distance between voxel boundaries along each axis
  glm::vec3 tDelta((ray_dir.x != 0.0f) ? std::abs(1.0f / ray_dir.x)
                                       : std::numeric_limits<float>::infinity(),
                   (ray_dir.y != 0.0f) ? std::abs(1.0f / ray_dir.y)
                                       : std::numeric_limits<float>::infinity(),
                   (ray_dir.z != 0.0f)
                       ? std::abs(1.0f / ray_dir.z)
                       : std::numeric_limits<float>::infinity());

  float dist = 0.0f;
  face_normal = glm::ivec3(0);

  while (dist <= max_dist) {
    // Check voxel
    if (getBlock(voxel.x, voxel.y, voxel.z) != BlockType::AIR) {
      block_pos = voxel;
      return true;
    }

    // Step to next voxel
    if (tMax.x < tMax.y) {
      if (tMax.x < tMax.z) {
        voxel.x += step.x;
        dist = tMax.x;
        tMax.x += tDelta.x;
        face_normal = glm::ivec3(-step.x, 0, 0);
      } else {
        voxel.z += step.z;
        dist = tMax.z;
        tMax.z += tDelta.z;
        face_normal = glm::ivec3(0, 0, -step.z);
      }
    } else {
      if (tMax.y < tMax.z) {
        voxel.y += step.y;
        dist = tMax.y;
        tMax.y += tDelta.y;
        face_normal = glm::ivec3(0, -step.y, 0);
      } else {
        voxel.z += step.z;
        dist = tMax.z;
        tMax.z += tDelta.z;
        face_normal = glm::ivec3(0, 0, -step.z);
      }
    }
  }

  return false;
}
