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

  int world_size = 5;

  for (int x = 0; x < world_size; x++) {
    for (int z = 0; z < world_size; z++) {
      addChunk(x, z);
    }
  }
}

void World::addChunk(int x, int z) {
  ChunkKey key{x, z};
  thread_pool.enqueue([this, x, z, key]() {
    auto chunk = std::make_unique<Chunk>(x, z);
    chunk->uploadGPU(renderer->getTextureManager());
    {
      std::lock_guard<std::mutex> lk(chunks_mutex);
      chunks.emplace(key, chunk);
    }
  });
}

Chunk *World::getChunk(int x, int z) {
  ChunkKey key{x, z};
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    return it->second.get();
  }
  return nullptr;
}

const Chunk *World::getChunk(int x, int z) const {
  ChunkKey key{x, z};
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    return it->second.get();
  }
  return nullptr;
}

void World::update(float) {}

void World::render(glm::mat4 &view, glm::mat4 &projection) {
  renderer->drawChunks(chunks, view, projection);
}
