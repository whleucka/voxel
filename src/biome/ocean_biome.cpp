#include "biome/ocean_biome.hpp"

OceanBiome::OceanBiome() {
}

BlockType OceanBiome::generateInternalBlock(int, int, int) {
    return BlockType::WATER;
}

BlockType OceanBiome::generateTopBlock() {
    return BlockType::WATER;
}

void OceanBiome::spawnDecorations(Chunk&) {
}
