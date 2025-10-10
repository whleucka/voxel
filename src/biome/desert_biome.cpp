#include "biome/desert_biome.hpp"
#include "biome/palm_tree_generator.hpp"
#include "block/block_type.hpp"
#include <memory>

DesertBiome::DesertBiome()
    : palm_tree_spawner(0.98, std::make_unique<PalmTreeGenerator>()) {}

BlockType DesertBiome::generateTopBlock(int) { return BlockType::SAND; }

void DesertBiome::spawnDecorations(Chunk &c) {
  palm_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    return chunk.at(x, y, z) == BlockType::SAND;
  });
}
