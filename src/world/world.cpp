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
  std::lock_guard<std::mutex> lk(chunks_mutex);
  // Generate the pending chunk meshes
  while (!mesh_queue.empty()) {
    auto chunk = mesh_queue.front();
    mesh_queue.pop();
    chunk->generateMesh(renderer->getTextureManager());
    // Queue for GPU upload
    upload_queue.push(chunk);
  }
  while (!upload_queue.empty()) {
    // Send chunk to GPU for rendering
    upload_queue.front()->uploadGPU();
    upload_queue.pop();
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
