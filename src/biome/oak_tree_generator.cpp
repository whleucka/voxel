#include "biome/oak_tree_generator.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"

void OakTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  auto safe_set = [&](int bx, int by, int bz, BlockType type) {
    if (bx >= 0 && bx < kChunkWidth && by >= 0 && by < kChunkHeight &&
        bz >= 0 && bz < kChunkDepth) {
      chunk.at(bx, by, bz) = type;
    }
  };

  auto safe_set_leaf = [&](int bx, int by, int bz) {
    if (bx >= 0 && bx < kChunkWidth && by >= 0 && by < kChunkHeight &&
        bz >= 0 && bz < kChunkDepth) {
      BlockType cur = chunk.at(bx, by, bz);
      if (cur == BlockType::AIR || cur == BlockType::OAK_LEAF)
        chunk.at(bx, by, bz) = BlockType::OAK_LEAF;
    }
  };

  if (y <= 0 || y >= kChunkHeight - 16) return;

  // --- Trunk ---
  int h = (rand() % 3) + 5; // height 5-7
  for (int j = 1; j <= h; j++) {
    safe_set(x, y + j, z, BlockType::OAK_LOG);
  }

  // --- Canopy: layered approach for a natural rounded crown ---
  int crown_base = y + h - 2;   // leaves start 2 below the trunk top
  int crown_top  = y + h + 2;   // leaves extend 2 above the trunk top

  for (int cy = crown_base; cy <= crown_top; ++cy) {
    // Determine radius for this layer — widest in the middle, narrow at
    // top and bottom (oblate sphere / rounded crown shape)
    float t = static_cast<float>(cy - crown_base) /
              static_cast<float>(crown_top - crown_base);
    // Parabolic profile: widest at t ~ 0.4 (slightly below center)
    float shape = 1.0f - (t - 0.4f) * (t - 0.4f) / 0.36f;
    if (shape < 0.0f) shape = 0.0f;

    int radius = static_cast<int>(2.0f + shape * 2.0f); // 2 to 4

    for (int dx = -radius; dx <= radius; ++dx) {
      for (int dz = -radius; dz <= radius; ++dz) {
        float dist_sq = static_cast<float>(dx * dx + dz * dz);
        float r_sq = static_cast<float>(radius * radius);

        // Skip corners for rounder shape
        if (dist_sq > r_sq) continue;

        // Slight random skip at the edges only (10% chance) for natural look
        if (dist_sq > r_sq * 0.7f && rand() % 10 == 0) continue;

        safe_set_leaf(x + dx, cy, z + dz);
      }
    }
  }
}
