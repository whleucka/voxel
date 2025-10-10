#include "biome/mountain_biome.hpp"
#include "biome/oak_tree_generator.hpp"
#include "biome/pine_tree_generator.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"

MountainBiome::MountainBiome()
    : oak_tree_spawner(0.85, std::make_unique<OakTreeGenerator>()),
      pine_tree_spawner(0.7, std::make_unique<PineTreeGenerator>()) {}

BlockType MountainBiome::generateTopBlock(int) {
  int rando = rand() % 10 + 1;
  return rando >= 7 ? BlockType::DIRT : BlockType::GRASS;
}

void MountainBiome::spawnDecorations(Chunk &c) {
  pine_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    return chunk.at(x, y, z) == BlockType::GRASS && y < kTreeLevel;
  });
  oak_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    return chunk.at(x, y, z) == BlockType::GRASS && y < kTreeLevel;
  });
}
