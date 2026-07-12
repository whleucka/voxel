#include "biome/cherry_biome.hpp"
#include "biome/cherry_tree_generator.hpp"
#include "block/block_type.hpp"
#include <memory>

// Denser than the other biomes' spawners (oak in plains is 0.85) so the grove
// reads as a continuous blossom canopy rather than scattered lone trees.
//
// Only one spawner here on purpose: TreeSpawner samples the same noise field at
// the same world coordinates for every spawner, so a second species in this
// biome would spawn at a strict subset of these positions — i.e. growing
// straight through the cherries — rather than interleaving with them.
CherryBiome::CherryBiome()
    : cherry_tree_spawner(0.62, std::make_unique<CherryTreeGenerator>()) {}

BlockType CherryBiome::generateTopBlock(int) { return BlockType::GRASS; }

void CherryBiome::spawnDecorations(Chunk &c) {
  cherry_tree_spawner.spawn(c, [](Chunk &chunk, int x, int y, int z) {
    return chunk.at(x, y, z) == BlockType::GRASS;
  });
}
