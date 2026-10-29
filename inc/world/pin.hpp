#pragma once

#include <cstdint>
#include <glm/glm.hpp>

// A coloured marker a player dropped on their map (a cave mouth, a good spot)
// at the block their feet were in. Each player has their own, kMaxPins at
// most, and no two of a player's pins share a colour.
constexpr int kMaxPins = 8;

struct Pin {
  glm::ivec3 pos{0};
  uint8_t colour = 0; // 0 .. kMaxPins - 1
  bool operator==(const Pin &) const = default;
};
