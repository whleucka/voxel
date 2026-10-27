#include "ui/inventory_screen.hpp"
#include "item/smelting.hpp"

namespace {

bool isContainerSlot(int slot) { return slot >= kFurnaceInput && slot <= kFurnaceOutput; }

// Somewhere for `moving` among inventory slots [begin, end): a stack of the
// same item with room, else an empty slot.
std::optional<int> roomFor(const ItemStack &moving, const Inventory::Slots &inv, int begin,
                           int end) {
  for (int i = begin; i < end; ++i)
    if (inv[i].id == moving.id && inv[i].space() > 0) return i;
  for (int i = begin; i < end; ++i)
    if (inv[i].empty()) return i;
  return std::nullopt;
}

} // namespace

void InventoryScreen::setContainer(std::optional<Container> c) {
  container_ = std::move(c);
  if (!container_ && held_ && isContainerSlot(*held_)) held_.reset();
}

bool InventoryScreen::usable(MaybeSlot slot) const {
  if (!slot) return false;
  return Inventory::validSlot(*slot) || (container_ && isContainerSlot(*slot));
}

const ItemStack &InventoryScreen::at(int slot, const Inventory::Slots &inv) const {
  return isContainerSlot(slot) ? (*container_)[slot - kFurnaceInput] : inv[slot];
}

std::optional<MoveItems> InventoryScreen::leftClick(MaybeSlot slot,
                                                    const Inventory::Slots &inv) {
  if (!held_) {
    if (usable(slot) && !at(*slot, inv).empty()) held_ = *slot;
    return std::nullopt;
  }
  int from = *held_;
  held_.reset();
  // Off the grid, the source itself, or into a furnace's output: cancel.
  if (!usable(slot) || *slot == from || *slot == kFurnaceOutput) return std::nullopt;
  return MoveItems{from, *slot, 0};
}

std::optional<MoveItems> InventoryScreen::rightClick(MaybeSlot slot,
                                                     const Inventory::Slots &) {
  if (!held_) return std::nullopt;
  if (!usable(slot)) {
    held_.reset(); // off the grid cancels, as with a left click
    return std::nullopt;
  }
  if (*slot == *held_ || *slot == kFurnaceOutput) return std::nullopt;
  // Keep holding: sync() lets go once the source runs out.
  return MoveItems{*held_, *slot, 1};
}

std::optional<MoveItems> InventoryScreen::shiftClick(MaybeSlot slot,
                                                     const Inventory::Slots &inv) {
  held_.reset();
  if (!usable(slot) || at(*slot, inv).empty()) return std::nullopt;
  const ItemStack &moving = at(*slot, inv);

  if (isContainerSlot(*slot)) {
    if (auto to = roomFor(moving, inv, 0, kInventorySlots)) return MoveItems{*slot, *to, 0};
    return std::nullopt;
  }

  if (container_) {
    auto fits = [&](int target) {
      const ItemStack &there = (*container_)[target - kFurnaceInput];
      return there.empty() || (there.id == moving.id && there.space() > 0);
    };
    if (smeltResult(moving.id) && fits(kFurnaceInput)) return MoveItems{*slot, kFurnaceInput, 0};
    if (fuelMs(moving.id) > 0 && fits(kFurnaceFuel)) return MoveItems{*slot, kFurnaceFuel, 0};
  }

  bool from_hotbar = *slot < kHotbarSlots;
  int begin = from_hotbar ? kHotbarSlots : 0;
  int end = from_hotbar ? kInventorySlots : kHotbarSlots;
  if (auto to = roomFor(moving, inv, begin, end)) return MoveItems{*slot, *to, 0};
  return std::nullopt;
}

void InventoryScreen::sync(const Inventory::Slots &inv) {
  if (held_ && (!usable(held_) || at(*held_, inv).empty())) held_.reset();
}
