#include "ui/inventory_screen.hpp"
#include "item/smelting.hpp"

namespace {

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
  bool was_container = held_ && isContainerSlot(*held_);
  container_ = std::move(c);
  if (was_container && !(container_ && isContainerSlot(*held_))) held_.reset();
}

bool InventoryScreen::isContainerSlot(int slot) const {
  int n = container_ ? static_cast<int>(container_->slots.size()) : kChestSlots;
  return slot >= kContainerSlot0 && slot < kContainerSlot0 + n;
}

bool InventoryScreen::refuses(int slot) const {
  return container_ && container_->kind == ContainerKind::Furnace && slot == kFurnaceOutput;
}

bool InventoryScreen::usable(MaybeSlot slot) const {
  if (!slot) return false;
  return Inventory::validSlot(*slot) || (container_ && isContainerSlot(*slot));
}

const ItemStack &InventoryScreen::at(int slot, const Inventory::Slots &inv) const {
  return isContainerSlot(slot) ? container_->slots[slot - kContainerSlot0] : inv[slot];
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
  if (!usable(slot) || *slot == from || refuses(*slot)) return std::nullopt;
  return MoveItems{from, *slot, 0};
}

std::optional<MoveItems> InventoryScreen::rightClick(MaybeSlot slot,
                                                     const Inventory::Slots &) {
  if (!held_) return std::nullopt;
  if (!usable(slot)) {
    held_.reset(); // off the grid cancels, as with a left click
    return std::nullopt;
  }
  if (*slot == *held_ || refuses(*slot)) return std::nullopt;
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
    const std::vector<ItemStack> &there = container_->slots;
    auto fits = [&](int target) {
      const ItemStack &s = there[target - kContainerSlot0];
      return s.empty() || (s.id == moving.id && s.space() > 0);
    };
    if (container_->kind == ContainerKind::Chest) {
      // A stack of the same with room, else the first empty slot.
      for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < static_cast<int>(there.size()); ++i)
          if (pass == 0 ? there[i].id == moving.id && there[i].space() > 0 : there[i].empty())
            return MoveItems{*slot, kContainerSlot0 + i, 0};
      return std::nullopt; // the chest is full
    }
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
