#include "chunk/chunk.hpp"
#include "render/texture_manager.hpp"
#include "biome/biome_manager.hpp"
#include "core/constants.hpp"

Chunk::Chunk(int x, int z)
    : pos({x, z}),
      blocks(kChunkWidth * kChunkHeight * kChunkDepth, BlockType::AIR) {}



void Chunk::init() {
  BiomeType biome_type = BiomeManager::getBiomeForChunk(pos[0], pos[1]);
  std::unique_ptr<Biome> biome = BiomeManager::createBiome(biome_type);
  biome->generateTerrain(*this);
  biome->generateMinerals(*this);
  biome->fillWater(*this);
  biome->spawnDecorations(*this);
}

void Chunk::buildMeshData(World* world, TextureManager& texture_manager) {
  mesh.generateCPU(world, *this, texture_manager); // CPU-only
}

void Chunk::uploadGPU() {
  mesh.upload();
}

BlockType &Chunk::at(int x, int y, int z) {
  return blocks[x + kChunkWidth * (z + kChunkDepth * y)];
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
  return glm::translate(glm::mat4(1.0f),
                        glm::vec3(pos[0] * kChunkWidth, 0, pos[1] * kChunkDepth));
}
