#include "biome/tropical_biome.hpp"
#include "biome/oak_tree_generator.hpp"
#include "biome/palm_tree_generator.hpp"
#include "block/block_type.hpp"
#include <memory>

TropicalBiome::TropicalBiome()
    : oak_tree_spawner(0.90, std::make_unique<OakTreeGenerator>()),
      palm_tree_spawner(0.75, std::make_unique<PalmTreeGenerator>()) {}

BlockType TropicalBiome::generateTopBlock(int) { return BlockType::GRASS; }

void TropicalBiome::spawnDecorations(Chunk &c) {
  // Palm trees on sand or grass (tropical palms grow on beaches and inland)
  palm_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    BlockType top = chunk.at(x, y, z);
    return top == BlockType::SAND || top == BlockType::GRASS;
  });

  // Oak trees on grass
  oak_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    return chunk.at(x, y, z) == BlockType::GRASS;
  });
}
