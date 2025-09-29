#include "pine_tree_generator.hpp"
#include "block_type.hpp"

void PineTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  // set trunk
  int h = (rand() % 6) + 12;
  for (int j = 1; j < h; j++) {
    chunk.setBlock(x, y + j, z, BlockType::PINE_LOG);
  }

  // set leaves
  int leaf_height = h - 6;
  for (int i = 0; i < leaf_height; i++) {
    int radius = static_cast<int>((static_cast<float>(leaf_height - i) / leaf_height) * 4.0f);
    for (int dx = -radius; dx <= radius; dx++) {
      for (int dz = -radius; dz <= radius; dz++) {
        if (dx * dx + dz * dz <= radius * radius) {
          chunk.setBlock(x + dx, y + 6 + i, z + dz, BlockType::PINE_LEAF);
        }
      }
    }
  }
  chunk.setBlock(x, y + h, z, BlockType::PINE_LEAF);
}
