#include "biome/tropical_biome.hpp"

TropicalBiome::TropicalBiome() {
}

BlockType TropicalBiome::generateInternalBlock(int, int, int) {
    return BlockType::DIRT;
}

BlockType TropicalBiome::generateTopBlock(int) {
    return BlockType::GRASS;
}

void TropicalBiome::spawnDecorations(Chunk&) {
}
