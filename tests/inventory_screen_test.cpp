#include "doctest/doctest.h"
#include "ui/inventory_screen.hpp"

namespace {

const ItemId kDirt = itemForBlock(BlockType::DIRT);
const ItemId kSand = itemForBlock(BlockType::SAND);

bool same(const std::optional<MoveItems> &m, int from, int to, uint16_t count) {
  return m && m->from == from && m->to == to && m->count == count;
}

} // namespace

TEST_CASE("left-click picks up a stack, then drops it on another slot") {
  Inventory::Slots inv{};
  inv[3] = {kDirt, 10};
  InventoryScreen s;

  CHECK_FALSE(s.leftClick(3, inv)); // pick up: nothing sent yet
  CHECK(s.held() == 3);

  CHECK(same(s.leftClick(25, inv), 3, 25, 0));
  CHECK_FALSE(s.held());
}

TEST_CASE("left-click on an empty slot picks nothing up") {
  Inventory::Slots inv{};
  InventoryScreen s;
  CHECK_FALSE(s.leftClick(5, inv));
  CHECK_FALSE(s.held());
  CHECK_FALSE(s.leftClick(std::nullopt, inv));
  CHECK_FALSE(s.held());
}

TEST_CASE("clicking the source, off the grid, or closing cancels") {
  Inventory::Slots inv{};
  inv[0] = {kDirt, 4};
  InventoryScreen s;

  s.leftClick(0, inv);
  CHECK_FALSE(s.leftClick(0, inv));
  CHECK_FALSE(s.held());

  s.leftClick(0, inv);
  CHECK_FALSE(s.leftClick(std::nullopt, inv));
  CHECK_FALSE(s.held());

  s.leftClick(0, inv);
  CHECK_FALSE(s.rightClick(std::nullopt, inv));
  CHECK_FALSE(s.held());

  s.leftClick(0, inv);
  s.close();
  CHECK_FALSE(s.held());
}

TEST_CASE("right-click while holding drops one and keeps holding") {
  Inventory::Slots inv{};
  inv[2] = {kDirt, 3};
  InventoryScreen s;
  s.leftClick(2, inv);

  CHECK(same(s.rightClick(12, inv), 2, 12, 1));
  CHECK(s.held() == 2);
  CHECK(same(s.rightClick(13, inv), 2, 13, 1));
  CHECK(s.held() == 2);

  CHECK_FALSE(s.rightClick(2, inv)); // onto its own source: nothing
  CHECK(s.held() == 2);
}

TEST_CASE("right-click without holding does nothing") {
  Inventory::Slots inv{};
  inv[2] = {kDirt, 3};
  InventoryScreen s;
  CHECK_FALSE(s.rightClick(2, inv));
  CHECK_FALSE(s.held());
}

TEST_CASE("a pick-up is dropped once its source empties") {
  Inventory::Slots inv{};
  inv[2] = {kDirt, 1};
  InventoryScreen s;
  s.leftClick(2, inv);
  s.sync(inv);
  CHECK(s.held() == 2);

  inv[2] = {};
  s.sync(inv);
  CHECK_FALSE(s.held());
}

TEST_CASE("shift-click sends a hotbar stack to storage") {
  Inventory::Slots inv{};
  inv[0] = {kDirt, 5};
  InventoryScreen s;
  CHECK(same(s.shiftClick(0, inv), 0, kHotbarSlots, 0)); // first empty storage

  inv[17] = {kDirt, 60}; // a matching stack with room wins over empties
  CHECK(same(s.shiftClick(0, inv), 0, 17, 0));

  inv[17] = {kDirt, 64}; // full: falls back to the first empty
  CHECK(same(s.shiftClick(0, inv), 0, kHotbarSlots, 0));
}

TEST_CASE("shift-click sends a storage stack to the hotbar") {
  Inventory::Slots inv{};
  inv[0] = {kSand, 1};
  inv[30] = {kDirt, 5};
  InventoryScreen s;
  CHECK(same(s.shiftClick(30, inv), 30, 1, 0)); // slot 0 is sand
}

TEST_CASE("shift-click with no room, or on an empty slot, sends nothing") {
  Inventory::Slots inv{};
  for (int i = 0; i < kHotbarSlots; ++i) inv[i] = {kSand, 64};
  inv[30] = {kDirt, 5};
  InventoryScreen s;
  CHECK_FALSE(s.shiftClick(30, inv));
  CHECK_FALSE(s.shiftClick(31, inv));
  CHECK_FALSE(s.shiftClick(std::nullopt, inv));
}

TEST_CASE("shift-click cancels a pick-up") {
  Inventory::Slots inv{};
  inv[0] = {kDirt, 5};
  inv[1] = {kSand, 5};
  InventoryScreen s;
  s.leftClick(0, inv);
  s.shiftClick(1, inv);
  CHECK_FALSE(s.held());
}
