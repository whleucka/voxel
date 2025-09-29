#include "palm_tree_generator.hpp"
#include "block_type.hpp"

void PalmTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  // set trunk
  int h = (rand() % 6) + 12;
  for (int j = 1; j <= h; j++) {
    chunk.setBlock(x, y + j, z, BlockType::PALM_LOG);
  }

  // set leaves
  int top = y + h;

  // Top layer
  chunk.setBlock(x, top, z, BlockType::PALM_LEAF);

  // First layer of leaves
  chunk.setBlock(x + 1, top, z, BlockType::PALM_LEAF);
  chunk.setBlock(x - 1, top, z, BlockType::PALM_LEAF);
  chunk.setBlock(x, top, z + 1, BlockType::PALM_LEAF);
  chunk.setBlock(x, top, z - 1, BlockType::PALM_LEAF);

  // Second layer of leaves
  chunk.setBlock(x + 2, top - 1, z, BlockType::PALM_LEAF);
  chunk.setBlock(x - 2, top - 1, z, BlockType::PALM_LEAF);
  chunk.setBlock(x, top - 1, z + 2, BlockType::PALM_LEAF);
  chunk.setBlock(x, top - 1, z - 2, BlockType::PALM_LEAF);

  // Third layer of leaves
  chunk.setBlock(x + 3, top - 2, z, BlockType::PALM_LEAF);
  chunk.setBlock(x - 3, top - 2, z, BlockType::PALM_LEAF);
  chunk.setBlock(x, top - 2, z + 3, BlockType::PALM_LEAF);
  chunk.setBlock(x, top - 2, z - 3, BlockType::PALM_LEAF);

  // Diagonal leaves
  chunk.setBlock(x + 1, top - 1, z + 1, BlockType::PALM_LEAF);
  chunk.setBlock(x - 1, top - 1, z - 1, BlockType::PALM_LEAF);
  chunk.setBlock(x + 1, top - 1, z - 1, BlockType::PALM_LEAF);
  chunk.setBlock(x - 1, top - 1, z + 1, BlockType::PALM_LEAF);

  chunk.setBlock(x + 2, top - 2, z + 2, BlockType::PALM_LEAF);
  chunk.setBlock(x - 2, top - 2, z - 2, BlockType::PALM_LEAF);
  chunk.setBlock(x + 2, top - 2, z - 2, BlockType::PALM_LEAF);
  chunk.setBlock(x - 2, top - 2, z + 2, BlockType::PALM_LEAF);
}
