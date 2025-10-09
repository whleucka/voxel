#include "biome/ocean_biome.hpp"
#include "block/block_type.hpp"

OceanBiome::OceanBiome() {
}

BlockType OceanBiome::generateInternalBlock(int, int, int) {
    return BlockType::SANDSTONE;
}

BlockType OceanBiome::generateTopBlock(int y) {
    return BlockType::SANDSTONE;
}

void OceanBiome::spawnDecorations(Chunk&) {
}
