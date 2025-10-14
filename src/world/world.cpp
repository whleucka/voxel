#include "world/world.hpp"
#include "core/constants.hpp"
#include "render/renderer.hpp"
#include <memory>
#include <mutex>
#include "util/lock.hpp"

World::World()
    : renderer(std::make_unique<Renderer>()),
      player(std::make_unique<Player>(glm::vec3(25.0f, 125.0f, 25.0f))) {}

void World::init() {
  renderer->init();

  glm::vec3 playerPos = player->getPosition();
  last_chunk_x = static_cast<int>(floor(playerPos.x / kChunkWidth));
  last_chunk_z = static_cast<int>(floor(playerPos.z / kChunkDepth));

  updateLoadedChunks();
}

void World::addChunk(int x, int z) {
  ChunkKey key{x, z};
  auto chunk = std::make_shared<Chunk>(x, z);
  thread_pool.enqueue([this, chunk, key]() {
    // Init chunk (generate terrain, block types)
    chunk->init();
    {
      Lock lock(chunks_mutex);
      // Add new chunk
      chunks.emplace(key, chunk);
      // Queue for mesh generation
      mesh_queue.push(chunk);
    }
  });
}

void World::update(float dt) {
  player->update(dt);

  glm::vec3 pos = player->getPosition();
  int current_chunk_x = static_cast<int>(floor(pos.x / kChunkWidth));
  int current_chunk_z = static_cast<int>(floor(pos.z / kChunkDepth));

  if (current_chunk_x != last_chunk_x || current_chunk_z != last_chunk_z) {
    last_chunk_x = current_chunk_x;
    last_chunk_z = current_chunk_z;
    updateLoadedChunks();
  }

  std::queue<std::shared_ptr<Chunk>> mesh_local, upload_local;
  {
    Lock lock(chunks_mutex);
    std::swap(mesh_queue, mesh_local);
  }

  while (!mesh_local.empty()) {
    auto chunk = mesh_local.front();
    mesh_local.pop();
    chunk->generateMesh(this, renderer->getTextureManager());
    upload_local.push(chunk);
  }

  {
    Lock lock(chunks_mutex);
    std::swap(upload_queue, upload_local);
  }

  std::queue<std::shared_ptr<Chunk>> upload_copy;
  {
    Lock lock(chunks_mutex);
    std::swap(upload_queue, upload_copy);
  }

  while (!upload_copy.empty()) {
    upload_copy.front()->uploadGPU();
    upload_copy.pop();
  }
}

void World::updateLoadedChunks() {
  constexpr int r = kRenderDistance;
  robin_hood::unordered_set<ChunkKey, ChunkKeyHash> keep;

  for (int dx = -r; dx <= r; ++dx) {
    for (int dz = -r; dz <= r; ++dz) {
      if (dx * dx + dz * dz > r * r) continue;
      int cx = last_chunk_x + dx;
      int cz = last_chunk_z + dz;
      keep.insert({cx, cz});

      if (!getChunk(cx, cz)) {
        addChunk(cx, cz);
      }
    }
  }

  // unload chunks outside radius — but defer erasure safely
  std::vector<ChunkKey> to_remove;
  {
    Lock lock(chunks_mutex);
    for (auto &[key, _] : chunks) {
      if (!keep.count(key))
        to_remove.push_back(key);
    }

    for (auto &key : to_remove)
      chunks.erase(key);
  }
}

void World::unloadChunk(int x, int z) {
  Lock lock(chunks_mutex);
  chunks.erase({x, z});
}

void World::render(glm::mat4 &view, glm::mat4 &projection) {
  std::lock_guard<std::mutex> lk(chunks_mutex);
  renderer->drawChunks(chunks, view, projection);
}

std::shared_ptr<Chunk> World::getChunk(int x, int z) {
  return std::const_pointer_cast<Chunk>(std::as_const(*this).getChunk(x, z));
}

std::shared_ptr<const Chunk> World::getChunk(int x, int z) const {
  Lock lock(chunks_mutex);
  auto it = chunks.find({x, z});
  if (it == chunks.end()) return nullptr;
  return it->second;
}

size_t World::getChunkCount() const {
  Lock lock(chunks_mutex);
  return chunks.size();
}
