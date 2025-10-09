#include "biome/mountain_biome.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"

MountainBiome::MountainBiome() {}

BlockType MountainBiome::generateInternalBlock(int, int, int) {
  return BlockType::STONE;
}

BlockType MountainBiome::generateTopBlock(int y) {
  if (y > kSnowLevel) {
    return BlockType::SNOW;
  } else if (kSnowLevel >= kSnowLevel - 5) {
    return BlockType::SNOW_STONE;
  } else if (kSnowLevel >= kSnowLevel - 10) {
    return BlockType::STONE;
  }
  return BlockType::GRASS;
}

void MountainBiome::spawnDecorations(Chunk &) {}
