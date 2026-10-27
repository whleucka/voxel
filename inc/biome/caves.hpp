#pragma once

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

// Tunnels, carved right after the terrain (before ores, water and trees).
void carve(Chunk &chunk);

// Wall ores, glow crystals and pools, once the chunk is otherwise complete.
void decorate(Chunk &chunk);

// Whether carve() makes (x, y, z) in world space a cave. Slow: for tests.
bool isTunnel(glm::ivec3 world);

} // namespace caves
