#include "world/world.hpp"
#include "render/renderer.hpp"
#include <memory>
#include <mutex>

World::World() : renderer(new Renderer) {}

World::~World() {
  chunks.clear();
  delete renderer;
}

void World::init() {
  renderer->init();

  int world_size = 7;

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
      std::lock_guard<std::mutex> lk(chunks_mutex);
      // Add new chunk
      chunks.emplace(key, chunk);
      // Queue for mesh generation
      mesh_queue.push(chunk);
    }
  });
}

void World::update(float) {
  std::shared_ptr<Chunk> chunk;

  while (true) {
    {
      std::lock_guard<std::mutex> lk(chunks_mutex);
      if (mesh_queue.empty())
        break;
      chunk = mesh_queue.front();
      mesh_queue.pop();
    }
    chunk->generateMesh(renderer->getTextureManager());

    {
      std::lock_guard<std::mutex> lk(chunks_mutex);
      upload_queue.push(chunk);
    }
  }

  while (true) {
    {
      std::lock_guard<std::mutex> lk(chunks_mutex);
      if (upload_queue.empty())
        break;
      chunk = upload_queue.front();
      upload_queue.pop();
    }
    chunk->uploadGPU();
  }
}

void World::render(glm::mat4 &view, glm::mat4 &projection) {
  std::vector<std::shared_ptr<Chunk>> render_chunks;
  {
    std::lock_guard<std::mutex> lk(chunks_mutex);
    for (auto const &[key, val] : chunks) {
      render_chunks.push_back(val);
    }
  }
  renderer->drawChunks(render_chunks, view, projection);
}

std::shared_ptr<Chunk> World::getChunk(int x, int z) {
  ChunkKey key{x, z};
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<const Chunk> World::getChunk(int x, int z) const {
  ChunkKey key{x, z};
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    return it->second;
  }
  return nullptr;
}
