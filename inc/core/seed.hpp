#pragma once

#include <cstdint>
#include <glm/vec2.hpp>
#include <string_view>

// World seeds from text, as typed into voxel.properties or the new-world
// screen: blank = random (the clock), a number is used as-is, any other text
// is hashed.
uint32_t resolveSeed(std::string_view text);

// The noise offset a world of this seed uses: added to all noise coordinates.
// Capped at 5000 units: large enough for variety, small enough to avoid the
// all-ocean regions that appear at extreme offsets.
glm::vec2 noiseOffsetFor(uint32_t seed);
