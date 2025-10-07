#include "biome/mountain_biome.hpp"

MountainBiome::MountainBiome() {
}

BlockType MountainBiome::generateInternalBlock(int, int, int) {
    return BlockType::STONE;
}

BlockType MountainBiome::generateTopBlock() {
    return BlockType::GRASS;
}

void MountainBiome::spawnDecorations(Chunk&) {
}
