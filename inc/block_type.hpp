#pragma once

#include <cstdint>
#include <glm/vec2.hpp>

enum class BlockType : uint8_t {
  AIR = 0,
  BEDROCK,
  COBBLESTONE,
  DIRT,
  GRASS,
  SAND,
  SANDSTONE,
  SNOW,
  SNOW_DIRT,
  SNOW_STONE,
  STONE,
  TREE,
  TREE_LEAF,
  WATER,
  CLOUD,
};
