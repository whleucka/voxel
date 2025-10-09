#include "biome/plains_biome.hpp"

PlainsBiome::PlainsBiome() {
}

BlockType PlainsBiome::generateInternalBlock(int, int, int) {
    return BlockType::DIRT;
}

BlockType PlainsBiome::generateTopBlock(int) {
    return BlockType::GRASS;
}

void PlainsBiome::spawnDecorations(Chunk&) {
}
