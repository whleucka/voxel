#pragma once

#include "block/block_data.hpp"
#include "block/stairs.hpp"
#include <glm/glm.hpp>

// A slab or a stair in water: the water fills the half of the cell it
// leaves, as it would flow into an empty cell there. The Game lets water flow
// on out of such a half (Game::waterFlowsInto), and the mesh draws it as water
// (ChunkMesh), so the two must agree: both ask this.
//
// A bottom slab or a stair leaves its top open, and sits on itself: water
// beside it or over it (or in the open bottom of a top slab over it) fills
// that. A top slab leaves its bottom open, under its solid top: water beside
// it fills that, if there's something under it to sit on (as for an empty
// cell: water doesn't spread out across the air beside a fall).
//
// `at(x, y, z)` gives the block in a cell.
template <class At> bool halfShapeWet(const At &at, int x, int y, int z) {
  const BlockType b = at(x, y, z);
  if (!isHalfShape(b)) return false;
  auto waterBeside = [&] {
    for (const glm::ivec3 &s : kSides)
      if (isLiquid(at(x + s.x, y, z + s.z))) return true;
    return false;
  };
  if (isTopSlab(b)) return at(x, y - 1, z) != BlockType::AIR && waterBeside();
  const BlockType above = at(x, y + 1, z);
  if (isLiquid(above)) return true;
  if (isTopSlab(above) && halfShapeWet(at, x, y + 1, z)) return true;
  return waterBeside();
}

// Water in a cell: a liquid, or the half a slab or a stair in water leaves.
template <class At> bool holdsWater(const At &at, int x, int y, int z) {
  return isLiquid(at(x, y, z)) || halfShapeWet(at, x, y, z);
}
