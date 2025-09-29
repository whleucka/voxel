#include "pine_tree_generator.hpp"
#include "block_type.hpp"

void PineTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  int h = (rand() % 6) + 12;

  // Trunk
  for (int j = 1; j < h; j++) {
    chunk.setBlock(x, y + j, z, BlockType::PINE_LOG);
  }

  // Leaves: conical taper
  int leaf_height = h - 3; // taller canopy
  for (int i = 0; i < leaf_height; i++) {
    // Taper radius more aggressively for conical shape
    int radius =
        static_cast<int>(((float)(leaf_height - i) / leaf_height) * 5.0f);

    // Don't let radius collapse to zero too soon
    if (radius < 1)
      radius = 1;

    int yy = y + i + 3; // start leaves higher up trunk
    for (int dx = -radius; dx <= radius; dx++) {
      for (int dz = -radius; dz <= radius; dz++) {
        if (dx * dx + dz * dz <= radius * radius) {
          // Random skip for natural unevenness
          if (rand() % 6 != 0) {
            chunk.setBlock(x + dx, yy, z + dz, BlockType::PINE_LEAF);
          }
        }
      }
    }
  }

  // Top spike (classic pine point)
  chunk.setBlock(x, y + h, z, BlockType::PINE_LEAF);
  chunk.setBlock(x, y + h + 1, z, BlockType::PINE_LEAF);
}
