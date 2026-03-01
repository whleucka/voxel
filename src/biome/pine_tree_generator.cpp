#include "biome/pine_tree_generator.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"

void PineTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
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
      if (cur == BlockType::AIR || cur == BlockType::PINE_LEAF)
        chunk.at(bx, by, bz) = BlockType::PINE_LEAF;
    }
  };

  if (y <= 0 || y >= kChunkHeight - 20) return;

  int h = (rand() % 5) + 10; // height 10-14

  // --- Trunk ---
  for (int j = 1; j <= h; j++) {
    safe_set(x, y + j, z, BlockType::PINE_LOG);
  }

  // --- Conical canopy with tiered layers ---
  // Leaves start about 40% up the trunk and taper to a point
  int canopy_start = h * 2 / 5;      // ~40% up
  int canopy_layers = h - canopy_start;

  for (int i = 0; i < canopy_layers; ++i) {
    int cy = y + canopy_start + i + 1;

    // Taper: widest at the bottom of canopy, narrowest at top
    float t = static_cast<float>(i) / static_cast<float>(canopy_layers);
    int radius = static_cast<int>((1.0f - t) * 4.0f + 0.5f);
    if (radius < 1) radius = 1;

    // Every other layer is slightly narrower for a tiered look
    if (i % 2 == 1 && radius > 1) radius -= 1;

    for (int dx = -radius; dx <= radius; ++dx) {
      for (int dz = -radius; dz <= radius; ++dz) {
        float dist_sq = static_cast<float>(dx * dx + dz * dz);
        float r_sq = static_cast<float>(radius * radius);

        // Circular footprint, skip corners
        if (dist_sq > r_sq) continue;

        // Light random skip at outer edges only (8% chance)
        if (dist_sq > r_sq * 0.6f && rand() % 12 == 0) continue;

        safe_set_leaf(x + dx, cy, z + dz);
      }
    }
  }

  // --- Top spike ---
  safe_set_leaf(x, y + h + 1, z);
  safe_set_leaf(x, y + h + 2, z);
}
