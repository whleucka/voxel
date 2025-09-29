#include "palm_tree_generator.hpp"
#include "block_type.hpp"

void PalmTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  int h = (rand() % 6) + 12;

  // Lean direction
  int dx = (rand() % 3) - 1;
  int dz = (rand() % 3) - 1;
  if (dx == 0 && dz == 0)
    dx = 1;

  int cx = x;
  int cz = z;

  for (int j = 1; j <= h; j++) {
    // Always place the vertical trunk piece
    chunk.setBlock(cx, y + j, cz, BlockType::PALM_LOG);

    if (j % 3 == 0) {
      // Place overlap block at the *next* position
      chunk.setBlock(cx + dx, y + j, cz + dz, BlockType::PALM_LOG);

      // Then move trunk position
      cx += dx;
      cz += dz;
    }
  }

  // Leaves on top
  int top = y + h;
  x = cx;
  z = cz;

  // same leaves code…
  chunk.setBlock(x, top, z, BlockType::PALM_LEAF);

  chunk.setBlock(x + 1, top, z, BlockType::PALM_LEAF);
  chunk.setBlock(x - 1, top, z, BlockType::PALM_LEAF);
  chunk.setBlock(x, top, z + 1, BlockType::PALM_LEAF);
  chunk.setBlock(x, top, z - 1, BlockType::PALM_LEAF);

  chunk.setBlock(x + 2, top - 1, z, BlockType::PALM_LEAF);
  chunk.setBlock(x - 2, top - 1, z, BlockType::PALM_LEAF);
  chunk.setBlock(x, top - 1, z + 2, BlockType::PALM_LEAF);
  chunk.setBlock(x, top - 1, z - 2, BlockType::PALM_LEAF);

  chunk.setBlock(x + 3, top - 2, z, BlockType::PALM_LEAF);
  chunk.setBlock(x - 3, top - 2, z, BlockType::PALM_LEAF);
  chunk.setBlock(x, top - 2, z + 3, BlockType::PALM_LEAF);
  chunk.setBlock(x, top - 2, z - 3, BlockType::PALM_LEAF);

  chunk.setBlock(x + 1, top - 1, z + 1, BlockType::PALM_LEAF);
  chunk.setBlock(x - 1, top - 1, z - 1, BlockType::PALM_LEAF);
  chunk.setBlock(x + 1, top - 1, z - 1, BlockType::PALM_LEAF);
  chunk.setBlock(x - 1, top - 1, z + 1, BlockType::PALM_LEAF);

  chunk.setBlock(x + 2, top - 2, z + 2, BlockType::PALM_LEAF);
  chunk.setBlock(x - 2, top - 2, z - 2, BlockType::PALM_LEAF);
  chunk.setBlock(x + 2, top - 2, z - 2, BlockType::PALM_LEAF);
  chunk.setBlock(x - 2, top - 2, z + 2, BlockType::PALM_LEAF);
}
