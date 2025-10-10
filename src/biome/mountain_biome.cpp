#include "biome/mountain_biome.hpp"
#include "block/block_type.hpp"

MountainBiome::MountainBiome() {}

BlockType MountainBiome::generateTopBlock(int) {
  return BlockType::DIRT;
}

void MountainBiome::spawnDecorations(Chunk &) {}
