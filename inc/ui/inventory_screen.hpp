#pragma once

#include "inventory/inventory.hpp"
#include "sim/protocol.hpp"
#include <optional>

// Click handling for the inventory screen. The inventory itself belongs to the
// Game, so "picking up" a stack is only a client-side marker on its source
// slot; the actual change is a single MoveItems when it is dropped. Nothing is
// in flight while a stack is held, so cancelling can never lose items.
//
// Slots are indices into Inventory::Slots; std::nullopt means "not on a slot".
class InventoryScreen {
public:
  using MaybeSlot = std::optional<int>;

  std::optional<int> held() const { return held_; }

  // Not holding: pick up a non-empty stack. Holding: drop it on another slot
  // (move/merge/swap); clicking the source or off the grid cancels.
  std::optional<MoveItems> leftClick(MaybeSlot slot, const Inventory::Slots &inv);
  // Holding: drop one item on another slot and keep holding the rest.
  std::optional<MoveItems> rightClick(MaybeSlot slot, const Inventory::Slots &inv);
  // Send a whole stack to the other section (hotbar <-> storage): first stack
  // of the same item with room, else first empty slot. Cancels any pick-up.
  std::optional<MoveItems> shiftClick(MaybeSlot slot, const Inventory::Slots &inv);

  // Call when a new inventory snapshot arrives: drops the pick-up if its
  // source has been emptied.
  void sync(const Inventory::Slots &inv);
  void close() { held_.reset(); }

private:
  std::optional<int> held_;
};
