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
  // Building blocks (creative palette only).
  OAK_PLANKS,
  PINE_PLANKS,
  PALM_PLANKS,
  CHERRY_PLANKS,
  BRICKS,
  STONE_BRICKS,
  GLASS,
  WHITE_BLOCK,
  RED_BLOCK,
  ORANGE_BLOCK,
  YELLOW_BLOCK,
  GREEN_BLOCK,
  BLUE_BLOCK,
  PURPLE_BLOCK,
  PINK_BLOCK,
  GLOW_CRYSTAL, // grows in caves (worldgen v2)
  CRAFTING_TABLE,
  FURNACE,
  FURNACE_LIT,  // a furnace while it burns: glows, and isn't an item
  CHEST,
  TREASURE_CHEST, // locked, in deep caves (worldgen v3): not an item, unbreakable
};

// The highest BlockType value. Update it when appending a type; a test in
// tests/item_test.cpp fails if a named type exists past it.
constexpr BlockType kLastBlockType = BlockType::TREASURE_CHEST;
