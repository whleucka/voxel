#include "biome/tropical_biome.hpp"
#include "biome/oak_tree_generator.hpp"
#include "biome/palm_tree_generator.hpp"
#include "block/block_type.hpp"
#include <memory>

TropicalBiome::TropicalBiome()
    : oak_tree_spawner(0.95, std::make_unique<OakTreeGenerator>()),
      palm_tree_spawner(0.8, std::make_unique<PalmTreeGenerator>()) {}

BlockType TropicalBiome::generateTopBlock(int) { return BlockType::GRASS; }

void TropicalBiome::spawnDecorations(Chunk &c) {
  palm_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    if (chunk.at(x, y, z) != BlockType::SAND) {
      return false;
    }

    if (chunk.at(x + 1, y, z) == BlockType::WATER ||
        chunk.at(x - 1, y, z) == BlockType::WATER ||
        chunk.at(x, y, z + 1) == BlockType::WATER ||
        chunk.at(x, y, z - 1) == BlockType::WATER) {
      return true;
    }

    return false;
  });

  oak_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    return chunk.at(x, y, z) == BlockType::GRASS;
  });
}
