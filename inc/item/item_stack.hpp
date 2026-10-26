#pragma once

#include "item/item.hpp"
#include <cstdint>

// An item and how many of it. An empty stack is always {NONE, 0}; the helpers
// below keep it that way.
struct ItemStack {
  ItemId id = ItemId::NONE;
  uint16_t count = 0;

  bool empty() const { return id == ItemId::NONE || count == 0; }
  // Room left before max_stack (an empty stack has room for a full stack of
  // anything, reported as the default max).
  uint16_t space() const;

  bool operator==(const ItemStack &) const = default;
};

// Move as much of src into dst as fits and return how many moved. An empty
// dst adopts src's id; different ids don't merge.
uint16_t mergeInto(ItemStack &dst, ItemStack &src);

// Remove up to n from s and return them as a new stack.
ItemStack split(ItemStack &s, uint16_t n);
