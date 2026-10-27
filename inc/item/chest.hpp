#pragma once

#include "item/item_stack.hpp"
#include <array>
#include <random>

// A chest's contents, and what a treasure chest holds when it's unlocked.
// Pure data; the Game owns the chests in the world.

constexpr int kChestSlots = 20; // 2 rows of 10, the width of the hotbar
using Chest = std::array<ItemStack, kChestSlots>;

constexpr int kTreasureMinStacks = 4;
constexpr int kTreasureMaxStacks = 6;

// One line of the loot table: an item, how many, and how likely.
struct TreasureLoot {
  ItemId id;
  uint16_t min, max;
  int weight;
};
const std::array<TreasureLoot, 11> &treasureLoot();

// A newly unlocked treasure chest: 4-6 different stacks from the loot
// table, picked by weight, scattered over its slots.
Chest rollTreasure(std::mt19937 &rng);
