#include "world/world.hpp"
#include "render/renderer.hpp"
#include <memory>
#include <mutex>

using Lock = std::lock_guard<std::mutex>;

World::World()
    : renderer(std::make_unique<Renderer>()),
      player(std::make_unique<Player>(glm::vec3(25.0f, 125.0f, 25.0f))) {}

void World::init() {
  renderer->init();

  int world_size = 25;

  for (int x = 0; x < world_size; x++) {
    for (int z = 0; z < world_size; z++) {
      addChunk(x, z);
    }
  }
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
  std::queue<std::shared_ptr<Chunk>> mesh_local, upload_local;
  {
    Lock lock(chunks_mutex);
    std::swap(mesh_queue, mesh_local);
  }

  while (!mesh_local.empty()) {
    auto chunk = mesh_local.front();
    mesh_local.pop();
    chunk->generateMesh(renderer->getTextureManager());
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

void World::render(glm::mat4 &view, glm::mat4 &projection) {
  std::lock_guard<std::mutex> lk(chunks_mutex);
  renderer->drawChunks(chunks, view, projection);
}

std::shared_ptr<Chunk> World::getChunk(int x, int z) {
  return std::const_pointer_cast<Chunk>(std::as_const(*this).getChunk(x, z));
}

std::shared_ptr<const Chunk> World::getChunk(int x, int z) const {
  auto it = chunks.find({x, z});
  return it != chunks.end() ? it->second : nullptr;
}
