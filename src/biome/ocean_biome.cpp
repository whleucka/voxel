#include "biome/ocean_biome.hpp"
#include "block/block_type.hpp"

OceanBiome::OceanBiome() {
}

BlockType OceanBiome::generateTopBlock(int y) {
    return BlockType::SANDSTONE;
}

void OceanBiome::spawnDecorations(Chunk&) {
}
