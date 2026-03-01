#include "biome/palm_tree_generator.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"
#include <cmath>

void PalmTreeGenerator::generate(Chunk &chunk, int x, int y, int z) {
  auto safe_set = [&](int bx, int by, int bz, BlockType type) {
    if (bx >= 0 && bx < kChunkWidth && by >= 0 && by < kChunkHeight &&
        bz >= 0 && bz < kChunkDepth) {
      if (chunk.at(bx, by, bz) == BlockType::AIR ||
          chunk.at(bx, by, bz) == BlockType::PALM_LEAF)
        chunk.at(bx, by, bz) = type;
    }
  };

  // --- Base checks ---
  if (y <= 0 || y >= kChunkHeight - 16) return;
  if (chunk.at(x, y - 1, z) == BlockType::AIR) return;

  int h = (rand() % 4) + 7; // height 7-10

  // Slight lean: pick a random direction and lean gently
  int lean_dx = (rand() % 3) - 1;
  int lean_dz = (rand() % 3) - 1;
  if (lean_dx == 0 && lean_dz == 0) lean_dx = 1;

  float fx = static_cast<float>(x);
  float fz = static_cast<float>(z);
  float lean_rate = 0.12f + (rand() % 3) * 0.04f; // 0.12 - 0.20

  // --- Trunk (slight lean, slowing near top) ---
  // Track previous integer position so we can fill gaps when the trunk
  // shifts diagonally.  Without this, a lean that crosses a 0.5 boundary
  // produces two log blocks that only share an edge, not a face.
  int prev_cx = x;
  int prev_cz = z;

  for (int j = 0; j < h; ++j) {
    int cx = static_cast<int>(std::round(fx));
    int cz = static_cast<int>(std::round(fz));
    int cy = y + j;

    // If both X and Z shifted at the same time, place a bridging log at
    // the previous Y level to keep the trunk face-connected.
    if (j > 0) {
      int step_x = cx - prev_cx;
      int step_z = cz - prev_cz;

      if (step_x != 0 && step_z != 0) {
        // Place a bridge block at the old Y, stepping one axis first
        safe_set(prev_cx + step_x, cy - 1, prev_cz, BlockType::PALM_LOG);
      } else if (step_x != 0 || step_z != 0) {
        // Single-axis shift: place an extra block at the current Y so the
        // horizontal step is connected through a shared face, not just an
        // edge with the block below.
        safe_set(prev_cx, cy, prev_cz, BlockType::PALM_LOG);
      }
    }

    safe_set(cx, cy, cz, BlockType::PALM_LOG);

    prev_cx = cx;
    prev_cz = cz;

    // Lean more in the lower portion, slow down near top (natural curve)
    float t = static_cast<float>(j) / static_cast<float>(h);
    float rate = lean_rate * (1.0f - t * 0.7f);
    fx += lean_dx * rate;
    fz += lean_dz * rate;
  }

  // --- Crown position (top of trunk) ---
  int top_x = prev_cx;
  int top_z = prev_cz;
  int top_y = y + h;

  // Crown cap: small cluster at the very top
  safe_set(top_x, top_y, top_z, BlockType::PALM_LEAF);
  safe_set(top_x + 1, top_y, top_z, BlockType::PALM_LEAF);
  safe_set(top_x - 1, top_y, top_z, BlockType::PALM_LEAF);
  safe_set(top_x, top_y, top_z + 1, BlockType::PALM_LEAF);
  safe_set(top_x, top_y, top_z - 1, BlockType::PALM_LEAF);

  // --- Fronds: 4 drooping arms radiating outward ---
  struct FrondDir { int dx, dz; };
  FrondDir frond_dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

  int frond_len = 3 + (rand() % 2); // 3-4 blocks long

  for (auto &dir : frond_dirs) {
    for (int i = 1; i <= frond_len; ++i) {
      // Droop: starts level at i=1, drops 1 block per 2 distance
      int droop = (i - 1) / 2;
      int lx = top_x + dir.dx * i;
      int ly = top_y - droop;
      int lz = top_z + dir.dz * i;

      safe_set(lx, ly, lz, BlockType::PALM_LEAF);

      // Add width to the frond (1 block on each side perpendicular)
      if (i <= frond_len - 1) {
        if (dir.dx != 0) {
          safe_set(lx, ly, lz + 1, BlockType::PALM_LEAF);
          safe_set(lx, ly, lz - 1, BlockType::PALM_LEAF);
        } else {
          safe_set(lx + 1, ly, lz, BlockType::PALM_LEAF);
          safe_set(lx - 1, ly, lz, BlockType::PALM_LEAF);
        }
      }
    }
  }

  // --- Diagonal fronds (4 more, shorter) for fullness ---
  FrondDir diag_dirs[4] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  int diag_len = 2 + (rand() % 2); // 2-3 blocks

  for (auto &dir : diag_dirs) {
    for (int i = 1; i <= diag_len; ++i) {
      int droop = (i) / 2;
      int lx = top_x + dir.dx * i;
      int ly = top_y - droop;
      int lz = top_z + dir.dz * i;

      safe_set(lx, ly, lz, BlockType::PALM_LEAF);
    }
  }
}
