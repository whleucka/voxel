#include "biome/desert_biome.hpp"

DesertBiome::DesertBiome() {
}

BlockType DesertBiome::generateTopBlock(int) {
    return BlockType::SAND;
}

void DesertBiome::spawnDecorations(Chunk&) {
}
