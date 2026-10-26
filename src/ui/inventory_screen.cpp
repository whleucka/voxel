#include "ui/inventory_screen.hpp"

namespace {

bool usable(InventoryScreen::MaybeSlot slot) {
  return slot && Inventory::validSlot(*slot);
}

} // namespace

std::optional<MoveItems> InventoryScreen::leftClick(MaybeSlot slot,
                                                    const Inventory::Slots &inv) {
  if (!held_) {
    if (usable(slot) && !inv[*slot].empty()) held_ = *slot;
    return std::nullopt;
  }
  int from = *held_;
  held_.reset();
  if (!usable(slot) || *slot == from) return std::nullopt; // cancel
  return MoveItems{from, *slot, 0};
}

std::optional<MoveItems> InventoryScreen::rightClick(MaybeSlot slot,
                                                     const Inventory::Slots &) {
  if (!held_) return std::nullopt;
  if (!usable(slot)) {
    held_.reset(); // off the grid cancels, as with a left click
    return std::nullopt;
  }
  if (*slot == *held_) return std::nullopt;
  // Keep holding: sync() lets go once the source runs out.
  return MoveItems{*held_, *slot, 1};
}

std::optional<MoveItems> InventoryScreen::shiftClick(MaybeSlot slot,
                                                     const Inventory::Slots &inv) {
  held_.reset();
  if (!usable(slot) || inv[*slot].empty()) return std::nullopt;

  const ItemStack &moving = inv[*slot];
  bool from_hotbar = *slot < kHotbarSlots;
  int begin = from_hotbar ? kHotbarSlots : 0;
  int end = from_hotbar ? kInventorySlots : kHotbarSlots;

  for (int i = begin; i < end; ++i)
    if (inv[i].id == moving.id && inv[i].space() > 0)
      return MoveItems{*slot, i, 0};
  for (int i = begin; i < end; ++i)
    if (inv[i].empty()) return MoveItems{*slot, i, 0};
  return std::nullopt;
}

void InventoryScreen::sync(const Inventory::Slots &inv) {
  if (held_ && inv[*held_].empty()) held_.reset();
}
