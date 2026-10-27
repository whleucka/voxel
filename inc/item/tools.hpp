#pragma once

#include "item/item.hpp"

// Tools and weapons: what kind, how good, how long they last and how hard
// they hit. Pure data; the mining rules are in item/mining.hpp.

enum class ToolKind : uint8_t { None, Pickaxe, Axe, Shovel, Sword };

// In order of strength: a tool meets a "needs at least" when tier >= it.
enum class Tier : uint8_t { Hand, Wood, Stone, Copper, Iron, Ruby, Emerald, Diamond };

struct Tool {
  ToolKind kind = ToolKind::None;
  Tier tier = Tier::Hand;
};

// What an item is as a tool (None/Hand for anything else, or nothing held).
Tool toolOf(ItemId id);
// The item for a kind and tier (Hand has none).
ItemId toolItem(ToolKind kind, Tier tier);

int miningSpeed(Tier tier); // how many times faster than a bare hand
int maxUses(Tier tier);     // before it breaks
// Uses before this item breaks: a tool's tier uses, the fishing rod's, or 0
// for anything that doesn't wear.
int usesOf(ItemId id);
int swordDamage(Tier tier); // hand: 1

// Damage a hit does with this held (or NONE): swords their tier's damage,
// axes one less, pickaxes and shovels 2, a bare hand or anything else 1.
int attackDamage(ItemId held);
