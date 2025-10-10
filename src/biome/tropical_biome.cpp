#include "biome/tropical_biome.hpp"

TropicalBiome::TropicalBiome() {
}

BlockType TropicalBiome::generateTopBlock(int) {
    return BlockType::GRASS;
}

void TropicalBiome::spawnDecorations(Chunk&) {
}
