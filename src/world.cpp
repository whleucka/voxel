#include "world.hpp"
#include "coord.hpp"
#include "render_ctx.hpp"
#include "aabb.hpp" // Include AABB header
#include "camera.hpp" // Include Camera header
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const int chunk_width = 16;
const int chunk_length = 16;
const int chunk_height = 256;
const int render_distance = 6;

inline int worldToChunkCoord(int pos, int chunkSize) {
  return (pos >= 0) ? (pos / chunkSize) : ((pos + 1) / chunkSize - 1);
}

World::World(Texture &a) : block_atlas(a) { start_threads(); }

World::~World() { stop_threads(); }

void World::loadChunk(int x, int z) {
  ChunkKey key{x, z};
  std::cout << "LOAD CHUNK (" << key.x << ", " << key.z << ")" << std::endl;

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
    if (_loading_q.count(key))
      return;
    _load_q.push(key);
    _loading_q.insert(key);
  }
  _load_q_cv.notify_one();
}

void World::unloadChunk(const ChunkKey &key) {
  std::cout << "UNLOAD CHUNK (" << key.x << ", " << key.z << ")" << std::endl;
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    cache[key] = it->second; // move into cache
    chunks.erase(it);        // remove from active
  }
}

int World::getChunkCount() const { return chunks.size(); }

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

  // Load any missing activeChunks in render distance
  for (int dx = -render_distance; dx <= render_distance; dx++) {
    for (int dz = -render_distance; dz <= render_distance; dz++) {
      int cx = cam_cx + dx;
      int cz = cam_cz + dz;

      ChunkKey key{cx, cz};
      if (chunks.find(key) == chunks.end() && _loading_q.find(key) == _loading_q.end()) {
        loadChunk(cx, cz);
      }
    }
  }

  // Unload chunks that are too far away
  std::vector<ChunkKey> toUnload;
  for (auto &[key, chunk] : chunks) {
    int distX = key.x - cam_cx;
    int distZ = key.z - cam_cz;
    if (std::abs(distX) > render_distance ||
        std::abs(distZ) > render_distance) {
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
      chunk->mesh.setupMesh();
      chunks[chunk->getChunkKey()] = chunk;
      _loading_q.erase(chunk->getChunkKey());
    }
  }
}

void World::draw(renderCtx &ctx) {
  if (chunks.empty())
    return;

  glUseProgram(ctx.block_shader.ID);

  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "view"), 1,
                     GL_FALSE, glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(ctx.proj));

  // Get frustum planes from the camera
  glm::vec4 frustumPlanes[6];
  // Assuming aspect, near, far are available from the camera or context
  // For now, using hardcoded values from Engine::render()
  float aspect = ctx.proj[1][1] / ctx.proj[0][0]; // Extract aspect from projection matrix
  float near = 0.5f;
  float far = 1024.0f;
  ctx.camera.getFrustumPlanes(frustumPlanes, aspect, near, far);

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
    chunk->draw(ctx.block_shader);
  }
}

BlockType World::getBlock(int x, int y, int z) {
  const int cx = worldToChunk(x, chunk_width);
  const int cz = worldToChunk(z, chunk_length);

  auto it = chunks.find(ChunkKey{cx, cz});
  if (it == chunks.end()) {
    // Heuristic: if chunk is not loaded, assume solid below a certain height, and air above.
    // This prevents internal faces from showing, and allows grass blocks at the edge to render.
    const int SEA_LEVEL = 53; // This should ideally be a global constant or passed in
    if (y < SEA_LEVEL) {
        return BlockType::STONE;
    } else {
        return BlockType::AIR;
    }
  }

  const int lx = worldToLocal(x, chunk_width);
  const int lz = worldToLocal(z, chunk_length);

  return it->second->getBlock(lx, y, lz);
}

// --- Thread Management ---

void World::start_threads() {
  const unsigned int num_threads = std::thread::hardware_concurrency();
  for (unsigned int i = 0; i < num_threads; ++i) {
    _threads.emplace_back(&World::thread_loop, this);
  }
}

void World::stop_threads() {
  {
    std::lock_guard<std::mutex> lock(_load_q_mutex);
    _should_stop = true;
  }
  _load_q_cv.notify_all();
  for (auto &t : _threads) {
    t.join();
  }
}

void World::thread_loop() {
  while (true) {
    ChunkKey key;
    {
      std::unique_lock<std::mutex> lock(_load_q_mutex);
      _load_q_cv.wait(lock, [this] { return _should_stop || !_load_q.empty(); });
      if (_should_stop) {
        return;
      }
      key = _load_q.front();
      _load_q.pop();
    }

    // Fresh generate
    Chunk *chunk = new Chunk(chunk_width, chunk_length, chunk_height, key.x, key.z, this);
    chunk->generateMesh(block_atlas);

    {
      std::lock_guard<std::mutex> lock(_generated_q_mutex);
      _generated_q.push(chunk);
    }
  }
}