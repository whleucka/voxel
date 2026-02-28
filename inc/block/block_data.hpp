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
    case BlockType::WATER:        return "Water";
    case BlockType::COAL_ORE:     return "Coal Ore";
    case BlockType::GOLD_ORE:     return "Gold Ore";
    case BlockType::DIAMOND_ORE:  return "Diamond Ore";
    case BlockType::IRON_ORE:     return "Iron Ore";
    case BlockType::EMERALD_ORE:  return "Emerald Ore";
    case BlockType::RUBY_ORE:     return "Ruby Ore";
    case BlockType::COPPER_ORE:   return "Copper Ore";
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
};
