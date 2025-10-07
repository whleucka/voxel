#pragma once

#include <cstdint>

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
  OAK_LOG,
  OAK_LEAF,
  PINE_LOG,
  PINE_LEAF,
  PALM_LOG,
  PALM_LEAF,
  WATER,
};
