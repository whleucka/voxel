#pragma once

#include "inventory/inventory.hpp"
#include "item/item_stack.hpp"
#include <bitset>
#include <cstdint>
#include <optional>
#include <vector>

// Crafting recipes. Pure data and logic: the Game decides who may craft
// what, and the crafting panel shows the same rules.
//
// A recipe's index in recipes() is its id on the wire, so recipes are only
// ever appended, like BlockType and ItemId.

// Every item id a player has ever had: block items are 1..255 and materials
// start at 256, so 512 covers both with room to grow.
constexpr size_t kItemIdSpace = 512;
using SeenItems = std::bitset<kItemIdSpace>;

inline void see(SeenItems &seen, ItemId id) {
  auto v = static_cast<size_t>(id);
  if (id != ItemId::NONE && v < kItemIdSpace) seen.set(v);
}

// `count` of any mix of the accepted items ("any planks").
struct Ingredient {
  std::vector<ItemId> any_of;
  uint16_t count = 1;
};

struct Recipe {
  ItemStack makes;
  std::vector<Ingredient> needs;
  bool needs_table = false; // a Crafting Table within reach
};

const std::vector<Recipe> &recipes();

// A recipe shows once every ingredient (one of its accepted items) has been seen.
bool knows(const Recipe &recipe, const SeenItems &seen);

// How many of the accepted items the inventory holds in all.
int countOf(const Inventory &inventory, const Ingredient &ingredient);

// How many times the ingredients alone would stretch (room not considered).
int timesAffordable(const Recipe &recipe, const Inventory &inventory);

// The inventory after crafting once: ingredients used up (storage before the
// hotbar, so what's in hand goes last) and the output added. Nothing if an
// ingredient is short or the output doesn't fit.
std::optional<Inventory> craftOnce(const Recipe &recipe, const Inventory &inventory);
