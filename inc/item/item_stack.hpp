#pragma once

#include "item/item.hpp"
#include <cstdint>

// An item and how many of it. An empty stack is always {NONE, 0}; the helpers
// below keep it that way.
struct ItemStack {
  ItemId id = ItemId::NONE;
  uint16_t count = 0;
  uint16_t wear = 0; // uses a tool has spent; stacks only merge at equal wear

  bool empty() const { return id == ItemId::NONE || count == 0; }
  // Room left before max_stack (an empty stack has room for a full stack of
  // anything, reported as the default max).
  uint16_t space() const;

  bool operator==(const ItemStack &) const = default;
};

// Move as much of src into dst as fits and return how many moved. An empty
// dst adopts src's id and wear; different ids or wear don't merge.
uint16_t mergeInto(ItemStack &dst, ItemStack &src);

// Remove up to n from s and return them as a new stack.
ItemStack split(ItemStack &s, uint16_t n);

// Moves count items (0 = the whole stack) from src to dst: into an empty
// dst, merged onto the same item (the remainder stays), or swapped with a
// different item when moving the whole stack. False, changing nothing, for
// anything else. The rule for every slot-to-slot move.
bool moveStack(ItemStack &src, ItemStack &dst, uint16_t count);
