#include "chunk/chunk.hpp"
#include "render/texture_manager.hpp"
#include "biome/biome_manager.hpp"
#include "core/constants.hpp"
#include <utility>

Chunk::Chunk(int x, int z)
    : pos({x, z}),
      blocks(kChunkWidth * kChunkHeight * kChunkDepth, BlockType::AIR) {}

Chunk::~Chunk() {}

void Chunk::init() {
  BiomeType biome_type = BiomeManager::getBiomeForChunk(pos[0], pos[1]);
  std::unique_ptr<Biome> biome = BiomeManager::createBiome(biome_type);
  biome->generateTerrain(*this);
  biome->fillWater(*this);
}

void Chunk::generateMesh(TextureManager &texture_manager) {
  mesh.generate(*this, texture_manager);
}

void Chunk::uploadGPU() {
  mesh.upload();
}

BlockType &Chunk::at(int x, int y, int z) {
  return const_cast<BlockType &>(
    std::as_const(*this).at(x, y, z)
  );
}

const BlockType &Chunk::at(int x, int y, int z) const {
  return blocks[x + kChunkWidth * (z + kChunkDepth * y)];
}

BlockType Chunk::safeAt(int x, int y, int z) const {
  if (x < 0 || x >= kChunkWidth || y < 0 || y >= kChunkHeight || z < 0 ||
      z >= kChunkDepth) {
    return BlockType::AIR;
  }
  return at(x, y, z);
}

glm::mat4 Chunk::getModelMatrix() const {
  glm::mat4 model(1.0f);
  glm::vec3 world_pos(pos[0] * kChunkWidth, 0, pos[1] * kChunkDepth);
  return glm::translate(model, world_pos);
}
