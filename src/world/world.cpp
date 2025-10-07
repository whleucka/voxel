#include "world/world.hpp"
#include "core/constants.hpp"
#include "render/renderer.hpp"
#include <memory>

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

void World::addChunk(int wx, int wz) {
  ChunkKey key{wx, wz};

  auto [chunk, inserted] = chunks.emplace(key, std::make_unique<Chunk>(wx, wz));

  if (inserted) {
    for (int x = 0; x < kChunkWidth; x++) {
      for (int z = 0; z < kChunkDepth; z++) {
        const int world_x = wx * kChunkWidth + x;
        const int world_z = wz * kChunkDepth + z;
        int h = terrain_manager.getHeight({world_x, world_z});
        for (int y = 0; y < h; y++) {
          if (y == h - 1) {
            // Top block
            chunk->second->at(x, y, z) = BlockType::GRASS;
          } else {
            chunk->second->at(x, y, z) = BlockType::DIRT;
          }
        }
      }
    }

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
