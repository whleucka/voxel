#pragma once

#include <cstdint>
#include <glm/vec2.hpp>

enum class BlockType : uint8_t {
  AIR = 0,
  DIRT,
  STONE,
  GRASS,
  WATER,
  BEDROCK,
  SAND,
  COBBLESTONE,
  SNOW,
  SNOW_STONE,
  SNOW_DIRT,
};

struct BlockInfo {
  bool solid;       // Blocks light & movement
  bool transparent; // Transparent block (separate pass)
  glm::vec2 uv;     // Texture atlas coordinates
};
