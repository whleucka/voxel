#include "world/world.hpp"
#include "render/renderer.hpp"
#include <memory>

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

void World::addChunk(int wx, int wz) {
  ChunkKey key{wx, wz};
  auto [chunk, inserted] = chunks.emplace(key, std::make_unique<Chunk>(wx, wz));

  if (inserted) {
    chunk->second->uploadGPU(renderer->getTextureManager());
  }
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
