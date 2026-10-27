#pragma once

#include "item/item_stack.hpp"
#include <optional>

// What a furnace does, as pure data and one pure step, so the Game only has
// to keep furnaces and tick them.

constexpr int kSmeltMs = 5000; // one item

// What one of `in` becomes, if it smelts at all.
std::optional<ItemId> smeltResult(ItemId in);
// How long one of `fuel` burns; 0 if it isn't fuel.
int fuelMs(ItemId fuel);

struct Furnace {
  ItemStack input, fuel, output;
  int burn_ms = 0;       // left on the fuel burning now
  int burn_total_ms = 0; // that fuel's full time (for the flame gauge)
  int cook_ms = 0;       // progress on the item cooking now

  bool lit() const { return burn_ms > 0; }
  bool operator==(const Furnace &) const = default;
};

struct FurnaceStep {
  bool lit_changed = false;   // lit -> out, or out -> lit
  bool slots_changed = false; // fuel used, or an item finished
};

// Runs a furnace for `ms`. A burn starts when there's something to cook,
// room for the result and fuel; once lit it burns to the end, cooking while
// it can (and pausing, not losing progress, while the output is full).
FurnaceStep tickFurnace(Furnace &furnace, int ms);
