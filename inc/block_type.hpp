#pragma once

#include <cstdint>

/**
 * block_type.hpp
 *
 * Defines a block type
 * 
 */
enum BlockType : uint8_t {
  AIR = 0,
  WATER, 
  GRASS,
  DIRT,
  STONE,
  BEDROCK,
  SAND,
  COBBLESTONE,
  SNOW,
  UNKNOWN  // for unloaded chunks
};
