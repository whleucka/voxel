#include "biome/cherry_tree_generator.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"

void CherryTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  auto safe_set = [&](int bx, int by, int bz, BlockType type) {
    if (bx >= 0 && bx < kChunkWidth && by >= 0 && by < kChunkHeight &&
        bz >= 0 && bz < kChunkDepth) {
      chunk.at(bx, by, bz) = type;
    }
  };

  auto safe_set_blossom = [&](int bx, int by, int bz) {
    if (bx >= 0 && bx < kChunkWidth && by >= 0 && by < kChunkHeight &&
        bz >= 0 && bz < kChunkDepth) {
      BlockType cur = chunk.at(bx, by, bz);
      if (cur == BlockType::AIR || cur == BlockType::CHERRY_LEAF)
        chunk.at(bx, by, bz) = BlockType::CHERRY_LEAF;
    }
  };

  if (y <= 0 || y >= kChunkHeight - 16) return;

  // --- Trunk ---
  // Shorter than an oak: a cherry reads as a wide crown on a stubby trunk.
  int h = (rand() % 2) + 4; // height 4-5
  for (int j = 1; j <= h; j++) {
    safe_set(x, y + j, z, BlockType::CHERRY_LOG);
  }

  int fork_y = y + h; // trunk top, where the branches split off

  // --- Branches: 3-4 arms angling up and outward from the fork ---
  // Each arm steps one block out and (every other step) one block up, so the
  // canopy sits on visible limbs rather than floating above a bare pole.
  struct Dir { int dx, dz; };
  const Dir dirs[8] = {{1, 0},  {-1, 0}, {0, 1},  {0, -1},
                       {1, 1},  {1, -1}, {-1, 1}, {-1, -1}};

  int branch_count = 3 + (rand() % 2); // 3-4 branches
  int start = rand() % 8;              // rotate which arms are used

  // Tips are where the blossom clusters get centred.
  struct Tip { int x, y, z; };
  Tip tips[4];

  for (int b = 0; b < branch_count; ++b) {
    const Dir &d = dirs[(start + b * 2) % 8];
    int len = 2 + (rand() % 2); // 2-3 blocks out

    int bx = x, by = fork_y, bz = z;
    for (int i = 1; i <= len; ++i) {
      bx += d.dx;
      bz += d.dz;
      if (i % 2 == 1) by += 1; // rise every other step
      safe_set(bx, by, bz, BlockType::CHERRY_LOG);
    }
    tips[b] = {bx, by, bz};
  }

  // --- Canopy: a blossom cluster over each branch tip ---
  // Overlapping clusters give the broad, billowy crown a cherry is known for,
  // instead of one uniform sphere.
  for (int b = 0; b < branch_count; ++b) {
    const Tip &t = tips[b];

    for (int cy = t.y - 1; cy <= t.y + 2; ++cy) {
      // Flat on the bottom, domed on top: widest just above the branch.
      int layer = cy - t.y;
      int radius = (layer <= 0) ? 2 : (layer == 1 ? 2 : 1);

      for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
          int dist_sq = dx * dx + dz * dz;
          int r_sq = radius * radius;

          if (dist_sq > r_sq) continue;

          // Thin the outer shell so the crown edge looks wispy, not solid.
          if (dist_sq > r_sq - 1 && rand() % 3 == 0) continue;

          safe_set_blossom(t.x + dx, cy, t.z + dz);
        }
      }
    }
  }

  // Cap the fork itself so the centre of the crown is not hollow.
  for (int cy = fork_y + 1; cy <= fork_y + 3; ++cy) {
    int radius = (cy == fork_y + 3) ? 1 : 2;
    for (int dx = -radius; dx <= radius; ++dx) {
      for (int dz = -radius; dz <= radius; ++dz) {
        if (dx * dx + dz * dz > radius * radius) continue;
        safe_set_blossom(x + dx, cy, z + dz);
      }
    }
  }
}
