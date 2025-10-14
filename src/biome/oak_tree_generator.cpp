#include "biome/oak_tree_generator.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"

void OakTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  // set trunk
  int h = (rand() % 4) + 8;
  for (int j = 1; j <= h; j++) {
    chunk.at(x, y + j, z) = BlockType::OAK_LOG;
  }

  // set leaves
  int radius = (rand() % 2) + 3;
  int top = y + h;
  int t_h = (rand() % 2) + h - radius; // leaf height
  int t_d = (rand() % 2) + 3;          // leaf depth

  for (int dy = -t_d; dy <= t_h; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      for (int dz = -radius; dz <= radius; dz++) {
        if (rand() % 100 > 69)
          continue;
        if (dx * dx + dz * dz + dy * dy <= radius * radius + 1) {
          int block_x = x + dx;
          int block_y = top + dy;
          int block_z = z + dz;
          if (block_x >= 0 && block_x < kChunkWidth && block_y >= 0 &&
              block_y < kChunkHeight && block_z >= 0 && block_z < kChunkDepth) {
            if (chunk.at(block_x, block_y, block_z) != BlockType::OAK_LOG)
              chunk.at(block_x, block_y, block_z) = BlockType::OAK_LEAF;
          }
        }
      }
    }
  }
}
