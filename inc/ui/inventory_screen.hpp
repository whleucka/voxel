#pragma once

#include "inventory/inventory.hpp"
#include "sim/protocol.hpp"
#include <array>
#include <optional>
#include <vector>

// Click handling for the inventory screen. The inventory itself belongs to the
// Game, so "picking up" a stack is only a client-side marker on its source
// slot; the actual change is a single MoveItems when it is dropped. Nothing is
// in flight while a stack is held, so cancelling can never lose items.
//
// Slots are indices into Inventory::Slots, or kContainerSlot0.. for an open
// furnace's or chest's slots; std::nullopt means "not on a slot".
class InventoryScreen {
public:
  using MaybeSlot = std::optional<int>;
  // What's being looked into: a furnace's input, fuel and output, or a
  // chest's kChestSlots.
  struct Container {
    ContainerKind kind = ContainerKind::Furnace;
    std::vector<ItemStack> slots;
    Container(ItemStack input, ItemStack fuel, ItemStack output)
        : slots{input, fuel, output} {}
    Container(ContainerKind k, std::vector<ItemStack> s) : kind(k), slots(std::move(s)) {}
  };

  // The furnace or chest being looked into (from ContainerView), or none.
  void setContainer(std::optional<Container> c);
  const std::optional<Container> &container() const { return container_; }

  std::optional<int> held() const { return held_; }

  // Not holding: pick up a non-empty stack. Holding: drop it on another slot
  // (move/merge/swap); clicking the source or off the grid cancels.
  std::optional<MoveItems> leftClick(MaybeSlot slot, const Inventory::Slots &inv);
  // Holding: drop one item on another slot and keep holding the rest.
  std::optional<MoveItems> rightClick(MaybeSlot slot, const Inventory::Slots &inv);
  // Send a whole stack to the other section (hotbar <-> storage): first stack
  // of the same item with room, else first empty slot. With a furnace open,
  // things that smelt go to its input and fuel to its fuel slot; with a
  // chest open, everything goes into the chest. A container's slots empty
  // into the inventory. Cancels any pick-up.
  std::optional<MoveItems> shiftClick(MaybeSlot slot, const Inventory::Slots &inv);

  // Call when a new inventory snapshot arrives: drops the pick-up if its
  // source has been emptied.
  void sync(const Inventory::Slots &inv);
  void close() { held_.reset(); }

private:
  std::optional<int> held_;
  std::optional<Container> container_;

  bool usable(MaybeSlot slot) const;
  bool isContainerSlot(int slot) const;
  // Only a furnace's output refuses things put into it.
  bool refuses(int slot) const;
  const ItemStack &at(int slot, const Inventory::Slots &inv) const;
};
