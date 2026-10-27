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

// ─── With a furnace open ───────────────────────────────────────────────

namespace {
InventoryScreen withFurnace(ItemStack input = {}, ItemStack fuel = {}, ItemStack output = {}) {
  InventoryScreen s;
  s.setContainer(InventoryScreen::Container{input, fuel, output});
  return s;
}
} // namespace

TEST_CASE("shift-click sends smeltables to the input and fuel to the fuel slot") {
  Inventory::Slots inv{};
  inv[12] = {ItemId::RAW_IRON, 5};
  inv[13] = {ItemId::COAL, 3};
  inv[14] = {kDirt, 8};
  InventoryScreen s = withFurnace();
  CHECK(same(s.shiftClick(12, inv), 12, kFurnaceInput, 0));
  CHECK(same(s.shiftClick(13, inv), 13, kFurnaceFuel, 0));
  CHECK(same(s.shiftClick(14, inv), 14, 0, 0)); // neither: storage -> hotbar as usual

  // A full input sends it the usual way instead.
  InventoryScreen busy = withFurnace({ItemId::RAW_GOLD, 1});
  CHECK(same(busy.shiftClick(12, inv), 12, 0, 0));
}

TEST_CASE("furnace slots pick up and shift-click into the inventory") {
  Inventory::Slots inv{};
  inv[0] = {ItemId::IRON_INGOT, 60};
  InventoryScreen s = withFurnace({}, {}, {ItemId::IRON_INGOT, 10});
  CHECK(same(s.shiftClick(kFurnaceOutput, inv), kFurnaceOutput, 0, 0)); // tops up first
  CHECK_FALSE(s.leftClick(kFurnaceOutput, inv));
  CHECK(s.held() == kFurnaceOutput);
  CHECK(same(s.leftClick(25, inv), kFurnaceOutput, 25, 0));
}

TEST_CASE("nothing is dropped into a furnace's output, and closing it lets go") {
  Inventory::Slots inv{};
  inv[3] = {ItemId::RAW_IRON, 2};
  InventoryScreen s = withFurnace({}, {}, {ItemId::IRON_INGOT, 1});
  s.leftClick(3, inv);
  CHECK_FALSE(s.leftClick(kFurnaceOutput, inv));
  CHECK_FALSE(s.held());

  s.leftClick(kFurnaceOutput, inv);
  REQUIRE(s.held() == kFurnaceOutput);
  s.setContainer(std::nullopt);
  CHECK_FALSE(s.held());
  CHECK_FALSE(s.leftClick(kFurnaceInput, inv)); // no furnace: not a slot
}

// ─── With a chest open ─────────────────────────────────────────────────

namespace {
InventoryScreen withChest(std::vector<ItemStack> slots = std::vector<ItemStack>(kChestSlots)) {
  InventoryScreen s;
  s.setContainer(InventoryScreen::Container{ContainerKind::Chest, std::move(slots)});
  return s;
}
} // namespace

TEST_CASE("shift-click fills a chest: same item first, then the first gap") {
  Inventory::Slots inv{};
  inv[12] = {ItemId::RAW_IRON, 5};
  inv[2] = {kDirt, 8};
  std::vector<ItemStack> chest(kChestSlots);
  chest[0] = {ItemId::COAL, 1};
  chest[6] = {ItemId::RAW_IRON, 60};
  InventoryScreen s = withChest(chest);
  CHECK(same(s.shiftClick(12, inv), 12, kContainerSlot0 + 6, 0)); // tops up the iron
  CHECK(same(s.shiftClick(2, inv), 2, kContainerSlot0 + 1, 0));   // dirt: first gap

  // A full chest takes nothing; the stack stays put.
  std::vector<ItemStack> full(kChestSlots, ItemStack{ItemId::COAL, 64});
  InventoryScreen packed = withChest(full);
  CHECK_FALSE(packed.shiftClick(2, inv));
}

TEST_CASE("chest slots take anything and empty into the inventory") {
  Inventory::Slots inv{};
  inv[3] = {kDirt, 4};
  std::vector<ItemStack> chest(kChestSlots);
  chest[2] = {ItemId::DIAMOND, 2};
  chest[19] = {ItemId::RUBY, 1};
  InventoryScreen s = withChest(chest);
  // Slot 2 is a furnace's output, but a chest's slot 2 takes things.
  s.leftClick(3, inv);
  CHECK(same(s.rightClick(kContainerSlot0 + 2, inv), 3, kContainerSlot0 + 2, 1));
  s.close();
  CHECK(same(s.shiftClick(kContainerSlot0 + 19, inv), kContainerSlot0 + 19, 0, 0));
  CHECK_FALSE(s.leftClick(kContainerSlot0 + kChestSlots, inv)); // past the last slot
  CHECK_FALSE(s.held());

  // A pick-up from the chest survives an update, and goes when it closes.
  s.leftClick(kContainerSlot0 + 2, inv);
  REQUIRE(s.held() == kContainerSlot0 + 2);
  s.setContainer(InventoryScreen::Container{ContainerKind::Chest, chest});
  CHECK(s.held() == kContainerSlot0 + 2);
  s.setContainer(std::nullopt);
  CHECK_FALSE(s.held());
}
