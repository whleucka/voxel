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
  COAL_ORE,
  GOLD_ORE,
  DIAMOND_ORE,
  IRON_ORE,
  EMERALD_ORE,
  RUBY_ORE,
  COPPER_ORE,
  GLOWSTONE,   // emits block light (see blockLightEmission)
  // Append new types at the end: the numeric value is persisted in world save
  // edit deltas, so reordering would remap blocks in existing saves.
  CHERRY_LOG,
  CHERRY_LEAF,
};
