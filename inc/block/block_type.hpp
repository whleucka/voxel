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
  // Doors: the state is in the type, in the order of block/door.hpp
  // (bottom then top half; facing N, E, S, W; closed then open).
  DOOR_BOTTOM_N_CLOSED, DOOR_BOTTOM_N_OPEN,
  DOOR_BOTTOM_E_CLOSED, DOOR_BOTTOM_E_OPEN,
  DOOR_BOTTOM_S_CLOSED, DOOR_BOTTOM_S_OPEN,
  DOOR_BOTTOM_W_CLOSED, DOOR_BOTTOM_W_OPEN,
  DOOR_TOP_N_CLOSED, DOOR_TOP_N_OPEN,
  DOOR_TOP_E_CLOSED, DOOR_TOP_E_OPEN,
  DOOR_TOP_S_CLOSED, DOOR_TOP_S_OPEN,
  DOOR_TOP_W_CLOSED, DOOR_TOP_W_OPEN,
  // Beds: the state is in the type, in the order of block/bed.hpp
  // (colour as the coloured blocks; foot then head; facing N, E, S, W).
  BED_WHITE_FOOT_N, BED_WHITE_FOOT_E, BED_WHITE_FOOT_S, BED_WHITE_FOOT_W,
  BED_WHITE_HEAD_N, BED_WHITE_HEAD_E, BED_WHITE_HEAD_S, BED_WHITE_HEAD_W,
  BED_RED_FOOT_N, BED_RED_FOOT_E, BED_RED_FOOT_S, BED_RED_FOOT_W,
  BED_RED_HEAD_N, BED_RED_HEAD_E, BED_RED_HEAD_S, BED_RED_HEAD_W,
  BED_ORANGE_FOOT_N, BED_ORANGE_FOOT_E, BED_ORANGE_FOOT_S, BED_ORANGE_FOOT_W,
  BED_ORANGE_HEAD_N, BED_ORANGE_HEAD_E, BED_ORANGE_HEAD_S, BED_ORANGE_HEAD_W,
  BED_YELLOW_FOOT_N, BED_YELLOW_FOOT_E, BED_YELLOW_FOOT_S, BED_YELLOW_FOOT_W,
  BED_YELLOW_HEAD_N, BED_YELLOW_HEAD_E, BED_YELLOW_HEAD_S, BED_YELLOW_HEAD_W,
  BED_GREEN_FOOT_N, BED_GREEN_FOOT_E, BED_GREEN_FOOT_S, BED_GREEN_FOOT_W,
  BED_GREEN_HEAD_N, BED_GREEN_HEAD_E, BED_GREEN_HEAD_S, BED_GREEN_HEAD_W,
  BED_BLUE_FOOT_N, BED_BLUE_FOOT_E, BED_BLUE_FOOT_S, BED_BLUE_FOOT_W,
  BED_BLUE_HEAD_N, BED_BLUE_HEAD_E, BED_BLUE_HEAD_S, BED_BLUE_HEAD_W,
  BED_PURPLE_FOOT_N, BED_PURPLE_FOOT_E, BED_PURPLE_FOOT_S, BED_PURPLE_FOOT_W,
  BED_PURPLE_HEAD_N, BED_PURPLE_HEAD_E, BED_PURPLE_HEAD_S, BED_PURPLE_HEAD_W,
  BED_PINK_FOOT_N, BED_PINK_FOOT_E, BED_PINK_FOOT_S, BED_PINK_FOOT_W,
  BED_PINK_HEAD_N, BED_PINK_HEAD_E, BED_PINK_HEAD_S, BED_PINK_HEAD_W,
};

// The highest BlockType value. Update it when appending a type; a test in
// tests/item_test.cpp fails if a named type exists past it.
constexpr BlockType kLastBlockType = BlockType::BED_PINK_HEAD_W;
