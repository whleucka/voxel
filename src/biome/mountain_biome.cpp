#include "biome/mountain_biome.hpp"
#include "block/block_type.hpp"
#include "biome/oak_tree_generator.hpp"
#include "core/constants.hpp"

MountainBiome::MountainBiome(): oak_tree_spawner(0.66, std::make_unique<OakTreeGenerator>())  {}

BlockType MountainBiome::generateTopBlock(int) {
  int rando = rand() % 10 + 1;
  return rando >= 7 ? BlockType::DIRT : BlockType::GRASS;
}

void MountainBiome::spawnDecorations(Chunk &c) {
  oak_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    return chunk.at(x, y, z) == BlockType::GRASS && y < kTreeLevel;
  });
}
