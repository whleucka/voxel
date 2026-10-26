#pragma once

#include <glm/glm.hpp>
#include <optional>
#include <vector>

class World;

// Walking paths over the block grid. A cell is where a creature's feet are:
// standable when the block below is solid and the creature's height in cells
// above it is air.
struct PathParams {
  int height_cells = 2;   // air needed above the ground
  int max_drop = 3;       // furthest it will step down
  int max_nodes = 600;    // cells explored before giving up
  float max_range = 32.0f; // horizontal distance limit, from -> to
};

bool standable(const World &world, glm::ivec3 cell, int height_cells);

// A* from `from` to `to`, moving to the 4 neighbours on the same level, up one
// block (a jump) or down at most max_drop. An endpoint in mid-air snaps down
// to the ground below it (within max_drop). Returns the cells to walk through,
// excluding `from` and ending at the goal; empty when already there. Nothing
// when there is no path within the limits.
std::optional<std::vector<glm::ivec3>> findPath(const World &world, glm::ivec3 from,
                                                glm::ivec3 to, const PathParams &params);
