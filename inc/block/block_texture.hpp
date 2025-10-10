#pragma once

#include "block/block_type.hpp"
#include <glm/glm.hpp>
#include "robin_hood/robin_hood.h"

struct BlockTexture {
  glm::ivec2 top;
  glm::ivec2 bottom;
  glm::ivec2 side;
};

static robin_hood::unordered_map<BlockType, BlockTexture> block_uv_map = {
    {BlockType::GRASS, {{0, 0}, {2, 0}, {1, 0}}},
    {BlockType::DIRT, {{2, 0}, {2, 0}, {2, 0}}},
    {BlockType::STONE, {{3, 0}, {3, 0}, {3, 0}}},
    {BlockType::BEDROCK, {{4, 0}, {4, 0}, {4, 0}}},
    {BlockType::SAND, {{5, 0}, {5, 0}, {5, 0}}},
    {BlockType::COBBLESTONE, {{6, 0}, {6, 0}, {6, 0}}},
    {BlockType::WATER, {{7, 0}, {7, 0}, {7, 0}}},
    {BlockType::SNOW, {{8, 0}, {8, 0}, {8, 0}}},
    {BlockType::SNOW_STONE, {{8, 0}, {9, 0}, {9, 0}}},
    {BlockType::SNOW_DIRT, {{8, 0}, {10, 0}, {10, 0}}},
    {BlockType::SANDSTONE, {{11, 0}, {11, 0}, {11, 0}}},
    {BlockType::OAK_LOG, {{14, 0}, {14, 0}, {13, 0}}},
    {BlockType::OAK_LEAF, {{12, 0}, {12, 0}, {12, 0}}},
    {BlockType::PINE_LOG, {{0, 1}, {0, 1}, {15, 0}}},
    {BlockType::PINE_LEAF, {{1, 1}, {1, 1}, {1, 1}}},
    {BlockType::PALM_LOG, {{3, 1}, {3, 1}, {2, 1}}},
    {BlockType::PINE_LEAF, {{4, 1}, {4, 1}, {4, 1}}},
    {BlockType::COAL_ORE, {{5, 1}, {5, 1}, {5, 1}}},
    {BlockType::GOLD_ORE, {{6, 1}, {6, 1}, {6, 1}}},
    {BlockType::DIAMOND_ORE, {{7, 1}, {7, 1}, {7, 1}}},
    {BlockType::IRON_ORE, {{8, 1}, {8, 1}, {8, 1}}},
    {BlockType::EMERALD_ORE, {{9, 1}, {9, 1}, {9, 1}}},
    {BlockType::RUBY_ORE, {{10, 1}, {10, 1}, {10, 1}}},
};
