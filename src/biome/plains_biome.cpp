#include "biome/plains_biome.hpp"
#include "biome/oak_tree_generator.hpp"
#include "block/block_type.hpp"
#include <memory>

PlainsBiome::PlainsBiome()
    : oak_tree_spawner(0.85, std::make_unique<OakTreeGenerator>()) {}

BlockType PlainsBiome::generateTopBlock(int) { return BlockType::GRASS; }

void PlainsBiome::spawnDecorations(Chunk &c) {
  oak_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    return chunk.at(x, y, z) == BlockType::GRASS;
  });
}
