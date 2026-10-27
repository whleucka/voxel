#pragma once

#include "core/constants.hpp"
#include <array>
#include <glm/glm.hpp>

class Chunk;

// Natural caves (worldgen v2): winding tunnels, gems on their walls, glow
// crystals and shallow pools. Everything here is a pure function of the
// world seed and world position, so caves cross chunk borders without seams
// and come out the same on every machine.
namespace caves {

constexpr int kFloor = 12;        // nothing is carved below this y
constexpr int kSafeRoof = 6;      // rock kept above a cave near water
constexpr int kOpenAbove = 4;     // caves may open where ground > sea + this

// Each column's ground height (blocks y < height are ground), as carve()
// found it, so decorate() needn't work it out again.
using Surfaces = std::array<int, kChunkWidth * kChunkDepth>;

// Tunnels, carved right after the terrain (before ores, water and trees).
void carve(Chunk &chunk, Surfaces &surfaces);

// Wall ores, glow crystals and pools, once the chunk is otherwise complete.
void decorate(Chunk &chunk, const Surfaces &surfaces);

// Treasure (worldgen v3): now and then a locked chest in a nook of a deep
// cave floor. Its own random stream, so the rest of the chunk is exactly v2.
constexpr int kTreasureDepth = 20;   // below sea level
constexpr unsigned kTreasureOdds = 40; // one chunk in this many gets one
void placeTreasure(Chunk &chunk, const Surfaces &surfaces);

// Whether carve() makes (x, y, z) in world space a cave. Slow: for tests.
bool isTunnel(glm::ivec3 world);

} // namespace caves
