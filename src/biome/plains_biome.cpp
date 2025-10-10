#include "biome/plains_biome.hpp"

PlainsBiome::PlainsBiome() {
}

BlockType PlainsBiome::generateTopBlock(int) {
    return BlockType::GRASS;
}

void PlainsBiome::spawnDecorations(Chunk&) {
}
