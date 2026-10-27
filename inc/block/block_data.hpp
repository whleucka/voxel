#pragma once

#include "block/block_type.hpp"
#include "robin_hood/robin_hood.h"
#include <glm/glm.hpp>
#include <string_view>

struct BlockTexture {
  glm::ivec2 top;
  glm::ivec2 bottom;
  glm::ivec2 side;
};

inline bool isTransparent(BlockType type) {
  return type == BlockType::WATER;
}

inline bool isFullyTransparent(BlockType type) {
  return type == BlockType::AIR;
}

// Liquid blocks only render top surface, not sides
inline bool isLiquid(BlockType type) {
  return type == BlockType::WATER;
}

// Cutout blocks discard the faint texels of their tile instead of drawing them,
// which turns the soft edges of the leaf art into see-through holes.  They stay
// in the opaque pass (depth writes on, no sorting needed) but must not cull the
// faces behind them, since you can see straight through the holes.
//
// The threshold cannot be shared: the four leaf tiles were drawn with very
// different alpha ranges, so one value would leave pine solid (its alpha floor
// is 159) while dissolving oak.  These classes each open up ~25% of their tile.
// The cut is never global — the water tile is a uniform alpha 153, so any
// threshold that opens the leaves would erase every water surface in the world.
//
// Encoded as a 2-bit class in BlockVertex::ao bits[7:6].  The matching alpha
// values live in the CUTOUT_THRESHOLD table in block.vert / shadow_depth.vert
// and must stay in the same order.
enum CutoutClass : uint8_t {
  kCutoutNone = 0,  // no alpha test
  kCutoutLow  = 1,  // 0.67 — oak, glass
  kCutoutMid  = 2,  // 0.80 — palm, cherry
  kCutoutHigh = 3,  // 0.92 — pine
};

inline uint8_t cutoutClass(BlockType type) {
  switch (type) {
    case BlockType::OAK_LEAF:
    case BlockType::GLASS:       return kCutoutLow;  // frame 255, pane 0
    case BlockType::PALM_LEAF:
    case BlockType::CHERRY_LEAF: return kCutoutMid;
    case BlockType::PINE_LEAF:   return kCutoutHigh;
    default:                     return kCutoutNone;
  }
}

inline bool isCutout(BlockType type) {
  return cutoutClass(type) != kCutoutNone;
}

// Determine if a face should be rendered between current block and neighbor
inline bool shouldRenderFace(BlockType current, BlockType neighbor) {
  if (neighbor == BlockType::AIR) return true;
  // Panes of glass side by side read as one window, so the face between them
  // is hidden.  Leaves keep their inner faces (below).
  if (current == BlockType::GLASS && neighbor == BlockType::GLASS) return false;
  // Cutout neighbours (leaves) have see-through holes, so the face behind them
  // is visible and must be emitted.  This includes leaf-against-leaf, which is
  // what gives the canopy interior depth instead of looking hollow through the
  // holes.
  if (isCutout(neighbor)) return true;
  if (isTransparent(current)) {
    // Transparent blocks: only render if neighbor is different type
    return neighbor != current;
  }
  // Opaque blocks: render if neighbor is transparent (visible through it)
  return isTransparent(neighbor);
}

// Returns the number of sky-light levels absorbed when light passes through
// a block.  0 = fully transparent (air), 15 = fully opaque (stone, dirt…).
inline uint8_t skyLightOpacity(BlockType type) {
  switch (type) {
    case BlockType::AIR:
    case BlockType::GLASS:     return 0;
    case BlockType::WATER:     return 2;
    case BlockType::OAK_LEAF:
    case BlockType::PINE_LEAF:
    case BlockType::PALM_LEAF:
    case BlockType::CHERRY_LEAF: return 1;
    default:                   return 15;
  }
}

// Light a block emits on its own, 0 (none) .. 15 (max, like glowstone).
// Emissive blocks seed the block-light BFS in Chunk::computeBlockLight().
inline uint8_t blockLightEmission(BlockType type) {
  switch (type) {
    case BlockType::GLOWSTONE:    return 15;
    case BlockType::GLOW_CRYSTAL: return 10; // softer than the lamp
    case BlockType::FURNACE_LIT:  return 13;
    default:                   return 0;
  }
}

inline std::string_view blockName(BlockType type) {
  switch (type) {
    case BlockType::AIR:          return "Air";
    case BlockType::BEDROCK:      return "Bedrock";
    case BlockType::COBBLESTONE:  return "Cobblestone";
    case BlockType::DIRT:         return "Dirt";
    case BlockType::GRASS:        return "Grass";
    case BlockType::SAND:         return "Sand";
    case BlockType::SANDSTONE:    return "Sandstone";
    case BlockType::SNOW:         return "Snow";
    case BlockType::SNOW_DIRT:    return "Snow Dirt";
    case BlockType::SNOW_STONE:   return "Snow Stone";
    case BlockType::STONE:        return "Stone";
    case BlockType::OAK_LOG:      return "Oak Log";
    case BlockType::OAK_LEAF:     return "Oak Leaf";
    case BlockType::PINE_LOG:     return "Pine Log";
    case BlockType::PINE_LEAF:    return "Pine Leaf";
    case BlockType::PALM_LOG:     return "Palm Log";
    case BlockType::PALM_LEAF:    return "Palm Leaf";
    case BlockType::CHERRY_LOG:   return "Cherry Log";
    case BlockType::CHERRY_LEAF:  return "Cherry Blossom";
    case BlockType::WATER:        return "Water";
    case BlockType::COAL_ORE:     return "Coal Ore";
    case BlockType::GOLD_ORE:     return "Gold Ore";
    case BlockType::DIAMOND_ORE:  return "Diamond Ore";
    case BlockType::IRON_ORE:     return "Iron Ore";
    case BlockType::EMERALD_ORE:  return "Emerald Ore";
    case BlockType::RUBY_ORE:     return "Ruby Ore";
    case BlockType::COPPER_ORE:   return "Copper Ore";
    case BlockType::GLOWSTONE:    return "Lamp";  // enum name kept for saves
    case BlockType::OAK_PLANKS:    return "Oak Planks";
    case BlockType::PINE_PLANKS:   return "Pine Planks";
    case BlockType::PALM_PLANKS:   return "Palm Planks";
    case BlockType::CHERRY_PLANKS: return "Cherry Planks";
    case BlockType::BRICKS:        return "Bricks";
    case BlockType::STONE_BRICKS:  return "Stone Bricks";
    case BlockType::GLASS:         return "Glass";
    case BlockType::WHITE_BLOCK:   return "White Block";
    case BlockType::RED_BLOCK:     return "Red Block";
    case BlockType::ORANGE_BLOCK:  return "Orange Block";
    case BlockType::YELLOW_BLOCK:  return "Yellow Block";
    case BlockType::GREEN_BLOCK:   return "Green Block";
    case BlockType::BLUE_BLOCK:    return "Blue Block";
    case BlockType::PURPLE_BLOCK:  return "Purple Block";
    case BlockType::PINK_BLOCK:    return "Pink Block";
    case BlockType::GLOW_CRYSTAL:  return "Glow Crystal";
    case BlockType::CRAFTING_TABLE: return "Crafting Table";
    case BlockType::FURNACE:       return "Furnace";
    case BlockType::FURNACE_LIT:   return "Lit Furnace";
    default:                      return "Unknown";
  }
}

static robin_hood::unordered_map<BlockType, BlockTexture> block_data = {
    {BlockType::GRASS, {glm::ivec2{0, 0}, glm::ivec2{2, 0}, glm::ivec2{1, 0}}},
    {BlockType::DIRT, {glm::ivec2{2, 0}, glm::ivec2{2, 0}, glm::ivec2{2, 0}}},
    {BlockType::STONE, {glm::ivec2{3, 0}, glm::ivec2{3, 0}, glm::ivec2{3, 0}}},
    {BlockType::BEDROCK, {glm::ivec2{4, 0}, glm::ivec2{4, 0}, glm::ivec2{4, 0}}},
    {BlockType::SAND, {glm::ivec2{5, 0}, glm::ivec2{5, 0}, glm::ivec2{5, 0}}},
    {BlockType::COBBLESTONE, {glm::ivec2{6, 0}, glm::ivec2{6, 0}, glm::ivec2{6, 0}}},
    {BlockType::WATER, {glm::ivec2{7, 0}, glm::ivec2{7, 0}, glm::ivec2{7, 0}}},
    {BlockType::SNOW, {glm::ivec2{8, 0}, glm::ivec2{8, 0}, glm::ivec2{8, 0}}},
    {BlockType::SNOW_STONE, {glm::ivec2{8, 0}, glm::ivec2{9, 0}, glm::ivec2{9, 0}}},
    {BlockType::SNOW_DIRT, {glm::ivec2{8, 0}, glm::ivec2{10, 0}, glm::ivec2{10, 0}}},
    {BlockType::SANDSTONE, {glm::ivec2{11, 0}, glm::ivec2{11, 0}, glm::ivec2{11, 0}}},
    {BlockType::OAK_LOG, {glm::ivec2{14, 0}, glm::ivec2{14, 0}, glm::ivec2{13, 0}}},
    {BlockType::OAK_LEAF, {glm::ivec2{12, 0}, glm::ivec2{12, 0}, glm::ivec2{12, 0}}},
    {BlockType::PINE_LOG, {glm::ivec2{0, 1}, glm::ivec2{0, 1}, glm::ivec2{15, 0}}},
    {BlockType::PINE_LEAF, {glm::ivec2{1, 1}, glm::ivec2{1, 1}, glm::ivec2{1, 1}}},
    {BlockType::PALM_LOG, {glm::ivec2{3, 1}, glm::ivec2{3, 1}, glm::ivec2{2, 1}}},
    {BlockType::PALM_LEAF, {glm::ivec2{4, 1}, glm::ivec2{4, 1}, glm::ivec2{4, 1}}},
    {BlockType::COAL_ORE, {glm::ivec2{5, 1}, glm::ivec2{5, 1}, glm::ivec2{5, 1}}},
    {BlockType::GOLD_ORE, {glm::ivec2{6, 1}, glm::ivec2{6, 1}, glm::ivec2{6, 1}}},
    {BlockType::DIAMOND_ORE, {glm::ivec2{7, 1}, glm::ivec2{7, 1}, glm::ivec2{7, 1}}},
    {BlockType::IRON_ORE, {glm::ivec2{8, 1}, glm::ivec2{8, 1}, glm::ivec2{8, 1}}},
    {BlockType::EMERALD_ORE, {glm::ivec2{9, 1}, glm::ivec2{9, 1}, glm::ivec2{9, 1}}},
    {BlockType::RUBY_ORE, {glm::ivec2{10, 1}, glm::ivec2{10, 1}, glm::ivec2{10, 1}}},
    {BlockType::COPPER_ORE, {glm::ivec2{11, 1}, glm::ivec2{11, 1}, glm::ivec2{11, 1}}},
    {BlockType::CHERRY_LEAF, {glm::ivec2{12, 1}, glm::ivec2{12, 1}, glm::ivec2{12, 1}}},
    {BlockType::CHERRY_LOG, {glm::ivec2{14, 1}, glm::ivec2{14, 1}, glm::ivec2{13, 1}}},
    // Building blocks fill row 2 on, in this order (tools/gen_building_tiles.py).
    {BlockType::OAK_PLANKS, {glm::ivec2{0, 2}, glm::ivec2{0, 2}, glm::ivec2{0, 2}}},
    {BlockType::PINE_PLANKS, {glm::ivec2{1, 2}, glm::ivec2{1, 2}, glm::ivec2{1, 2}}},
    {BlockType::PALM_PLANKS, {glm::ivec2{2, 2}, glm::ivec2{2, 2}, glm::ivec2{2, 2}}},
    {BlockType::CHERRY_PLANKS, {glm::ivec2{3, 2}, glm::ivec2{3, 2}, glm::ivec2{3, 2}}},
    {BlockType::BRICKS, {glm::ivec2{4, 2}, glm::ivec2{4, 2}, glm::ivec2{4, 2}}},
    {BlockType::STONE_BRICKS, {glm::ivec2{5, 2}, glm::ivec2{5, 2}, glm::ivec2{5, 2}}},
    {BlockType::GLASS, {glm::ivec2{6, 2}, glm::ivec2{6, 2}, glm::ivec2{6, 2}}},
    {BlockType::WHITE_BLOCK, {glm::ivec2{7, 2}, glm::ivec2{7, 2}, glm::ivec2{7, 2}}},
    {BlockType::RED_BLOCK, {glm::ivec2{8, 2}, glm::ivec2{8, 2}, glm::ivec2{8, 2}}},
    {BlockType::ORANGE_BLOCK, {glm::ivec2{9, 2}, glm::ivec2{9, 2}, glm::ivec2{9, 2}}},
    {BlockType::YELLOW_BLOCK, {glm::ivec2{10, 2}, glm::ivec2{10, 2}, glm::ivec2{10, 2}}},
    {BlockType::GREEN_BLOCK, {glm::ivec2{11, 2}, glm::ivec2{11, 2}, glm::ivec2{11, 2}}},
    {BlockType::BLUE_BLOCK, {glm::ivec2{12, 2}, glm::ivec2{12, 2}, glm::ivec2{12, 2}}},
    {BlockType::PURPLE_BLOCK, {glm::ivec2{13, 2}, glm::ivec2{13, 2}, glm::ivec2{13, 2}}},
    {BlockType::PINK_BLOCK, {glm::ivec2{14, 2}, glm::ivec2{14, 2}, glm::ivec2{14, 2}}},
    {BlockType::GLOWSTONE, {glm::ivec2{15, 2}, glm::ivec2{15, 2}, glm::ivec2{15, 2}}},
    {BlockType::GLOW_CRYSTAL, {glm::ivec2{0, 3}, glm::ivec2{0, 3}, glm::ivec2{0, 3}}},
    // A worktop on top, a tool rack round the sides, and the worktop again below.
    {BlockType::CRAFTING_TABLE, {glm::ivec2{1, 3}, glm::ivec2{1, 3}, glm::ivec2{2, 3}}},
    // A stone top and a mouth on every side; flames in it while lit.
    {BlockType::FURNACE, {glm::ivec2{3, 3}, glm::ivec2{3, 3}, glm::ivec2{4, 3}}},
    {BlockType::FURNACE_LIT, {glm::ivec2{3, 3}, glm::ivec2{3, 3}, glm::ivec2{5, 3}}},
};
