#include "biome/palm_tree_generator.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"

#include "biome/palm_tree_generator.hpp"
#include "block/block_type.hpp"
#include <cmath>

void PalmTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  // --- base checks ---
  if (chunk.at(x, y - 1, z) == BlockType::AIR) return; // avoid floating trees

  int h = (rand() % 6) + 10; // height 10–15
  float lean_strength = (rand() % 4 + 2) / 10.0f; // how much it bends (0.2–0.5)

  // Choose lean direction (biased toward coast if you want)
  int dx = (rand() % 3) - 1;
  int dz = (rand() % 3) - 1;
  if (dx == 0 && dz == 0) dx = 1;

  // fractional offsets for gradual lean
  float fx = (float)x;
  float fz = (float)z;

  // --- Trunk ---
  for (int j = 0; j < h; ++j) {
    int cx = (int)std::round(fx);
    int cz = (int)std::round(fz);
    int cy = y + j;

    // Only place if inside chunk bounds
    if (cy >= 0 && cy < kChunkHeight)
      chunk.at(cx, cy, cz) = BlockType::PALM_LOG;

    // Gradual horizontal lean
    fx += dx * lean_strength * 0.3f;
    fz += dz * lean_strength * 0.3f;

    // Add a slight curve upward near the top
    if (j > h * 0.7f)
      lean_strength *= 0.9f; // slows lean for a natural curve
  }

  // --- Leaves ---
  int top_y = y + h;
  int top_x = (int)std::round(fx);
  int top_z = (int)std::round(fz);

  // simple palm canopy pattern
  const int canopy[5][3] = {
    {0, 0, 0},
    {1, 0, 0}, {-1, 0, 0},
    {0, 0, 1}, {0, 0, -1}
  };

  for (auto &p : canopy)
    chunk.at(top_x + p[0], top_y + p[1], top_z + p[2]) = BlockType::PALM_LEAF;

  // extended leaves
  for (int i = 1; i <= 3; ++i) {
    chunk.at(top_x + i, top_y - 1, top_z) = BlockType::PALM_LEAF;
    chunk.at(top_x - i, top_y - 1, top_z) = BlockType::PALM_LEAF;
    chunk.at(top_x, top_y - 1, top_z + i) = BlockType::PALM_LEAF;
    chunk.at(top_x, top_y - 1, top_z - i) = BlockType::PALM_LEAF;
  }
}
