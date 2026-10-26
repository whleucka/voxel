#pragma once

#include "core/constants.hpp"
#include "item/item_stack.hpp"
#include <array>
#include <cstdint>

constexpr int kStorageSlots = 30; // 3 rows of 10 below the hotbar
constexpr int kInventorySlots = kHotbarSlots + kStorageSlots;

// A player's items. Slots 0..kHotbarSlots-1 are the hotbar, the rest are
// storage. Pure logic: the Game owns one per player and is the only thing
// that changes it.
class Inventory {
public:
  using Slots = std::array<ItemStack, kInventorySlots>;

  static bool validSlot(int i) { return i >= 0 && i < kInventorySlots; }

  const Slots &slots() const { return slots_; }
  const ItemStack &slot(int i) const { return slots_.at(i); }
  void set(int i, ItemStack s);

  // Tops up matching stacks first, then fills empty slots, both in slot
  // order (so the hotbar before storage). Returns what didn't fit.
  ItemStack add(ItemStack stack);

  // Removes up to n from a slot and returns them.
  ItemStack take(int i, uint16_t n);

  // Moves count items (0 = the whole stack) from one slot to another:
  // into an empty slot, merged onto the same item (remainder stays), or
  // swapped with a different item when moving the whole stack. Returns false,
  // leaving everything unchanged, for anything else.
  bool move(int from, int to, uint16_t count);

private:
  Slots slots_{};
};
