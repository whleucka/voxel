#include "biome/desert_biome.hpp"

DesertBiome::DesertBiome() {
}

BlockType DesertBiome::generateInternalBlock(int, int, int) {
    return BlockType::SANDSTONE;
}

BlockType DesertBiome::generateTopBlock() {
    return BlockType::SAND;
}

void DesertBiome::spawnDecorations(Chunk&) {
}
