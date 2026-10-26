#include "doctest/doctest.h"
#include "inventory/inventory.hpp"

namespace {

const ItemId kDirt = itemForBlock(BlockType::DIRT);
const ItemId kSand = itemForBlock(BlockType::SAND);
constexpr int kFirstStorage = kHotbarSlots;

int countOf(const Inventory &inv, ItemId id) {
  int n = 0;
  for (const ItemStack &s : inv.slots())
    if (s.id == id) n += s.count;
  return n;
}

} // namespace

TEST_CASE("layout: a 10-slot hotbar and 30 storage slots") {
  CHECK(kHotbarSlots == 10);
  CHECK(kInventorySlots == 40);
  Inventory inv;
  for (const ItemStack &s : inv.slots()) CHECK(s.empty());
}

// ─── add ───────────────────────────────────────────────────────────────

TEST_CASE("add fills the first empty slot, hotbar first") {
  Inventory inv;
  CHECK(inv.add({kDirt, 5}).empty());
  CHECK(inv.slot(0) == ItemStack{kDirt, 5});
}

TEST_CASE("add tops up matching stacks before using empty slots") {
  Inventory inv;
  inv.set(3, {kDirt, 60});
  inv.set(15, {kDirt, 10});
  CHECK(inv.add({kDirt, 20}).empty());
  CHECK(inv.slot(3) == ItemStack{kDirt, 64}); // topped up first (lowest slot)
  CHECK(inv.slot(15) == ItemStack{kDirt, 26});
  CHECK(inv.slot(0).empty());                 // no new slot used
}

TEST_CASE("add prefers a matching storage stack over an empty hotbar slot") {
  Inventory inv;
  inv.set(kFirstStorage, {kDirt, 1});
  CHECK(inv.add({kDirt, 1}).empty());
  CHECK(inv.slot(kFirstStorage) == ItemStack{kDirt, 2});
  CHECK(inv.slot(0).empty());
}

TEST_CASE("add overflows into storage once the hotbar is full") {
  Inventory inv;
  for (int i = 0; i < kHotbarSlots; ++i) inv.set(i, {kSand, 64});
  CHECK(inv.add({kDirt, 3}).empty());
  CHECK(inv.slot(kFirstStorage) == ItemStack{kDirt, 3});
}

TEST_CASE("add returns what doesn't fit") {
  Inventory inv;
  for (int i = 0; i < kInventorySlots; ++i) inv.set(i, {kSand, 64});
  inv.set(7, {kDirt, 62});
  ItemStack left = inv.add({kDirt, 5});
  CHECK(left == ItemStack{kDirt, 3});
  CHECK(inv.slot(7) == ItemStack{kDirt, 64});
}

TEST_CASE("add can spread one stack over several slots") {
  Inventory inv;
  CHECK(inv.add({kDirt, 64}).empty());
  CHECK(inv.add({kDirt, 64}).empty());
  CHECK(inv.add({kDirt, 10}).empty());
  CHECK(countOf(inv, kDirt) == 138);
  CHECK(inv.slot(0).count == 64);
  CHECK(inv.slot(1).count == 64);
  CHECK(inv.slot(2).count == 10);
}

// ─── take ──────────────────────────────────────────────────────────────

TEST_CASE("take removes up to n") {
  Inventory inv;
  inv.set(2, {kDirt, 3});
  CHECK(inv.take(2, 1) == ItemStack{kDirt, 1});
  CHECK(inv.take(2, 9) == ItemStack{kDirt, 2});
  CHECK(inv.slot(2).empty());
  CHECK(inv.take(2, 1).empty());
  CHECK(inv.take(-1, 1).empty());
  CHECK(inv.take(kInventorySlots, 1).empty());
}

// ─── move ──────────────────────────────────────────────────────────────

TEST_CASE("move into an empty slot") {
  Inventory inv;
  inv.set(0, {kDirt, 10});
  CHECK(inv.move(0, 20, 4));
  CHECK(inv.slot(0) == ItemStack{kDirt, 6});
  CHECK(inv.slot(20) == ItemStack{kDirt, 4});

  CHECK(inv.move(0, 21, 0)); // 0 = whole stack
  CHECK(inv.slot(0).empty());
  CHECK(inv.slot(21) == ItemStack{kDirt, 6});
}

TEST_CASE("move onto the same item merges and keeps the remainder") {
  Inventory inv;
  inv.set(0, {kDirt, 10});
  inv.set(1, {kDirt, 60});
  CHECK(inv.move(0, 1, 0));
  CHECK(inv.slot(1) == ItemStack{kDirt, 64});
  CHECK(inv.slot(0) == ItemStack{kDirt, 6});
}

TEST_CASE("move onto a full stack of the same item is rejected") {
  Inventory inv;
  inv.set(0, {kDirt, 10});
  inv.set(1, {kDirt, 64});
  CHECK_FALSE(inv.move(0, 1, 0));
  CHECK(inv.slot(0) == ItemStack{kDirt, 10});
  CHECK(inv.slot(1) == ItemStack{kDirt, 64});
}

TEST_CASE("moving a whole stack onto a different item swaps them") {
  Inventory inv;
  inv.set(0, {kDirt, 10});
  inv.set(5, {kSand, 3});
  CHECK(inv.move(0, 5, 0));
  CHECK(inv.slot(0) == ItemStack{kSand, 3});
  CHECK(inv.slot(5) == ItemStack{kDirt, 10});

  CHECK(inv.move(0, 5, 3)); // explicit count equal to the whole stack
  CHECK(inv.slot(0) == ItemStack{kDirt, 10});
}

TEST_CASE("moving part of a stack onto a different item is rejected") {
  Inventory inv;
  inv.set(0, {kDirt, 10});
  inv.set(5, {kSand, 3});
  CHECK_FALSE(inv.move(0, 5, 4));
  CHECK(inv.slot(0) == ItemStack{kDirt, 10});
  CHECK(inv.slot(5) == ItemStack{kSand, 3});
}

TEST_CASE("a count larger than the stack moves the whole stack") {
  Inventory inv;
  inv.set(0, {kDirt, 10});
  CHECK(inv.move(0, 1, 50));
  CHECK(inv.slot(1) == ItemStack{kDirt, 10});
  CHECK(inv.slot(0).empty());
}

TEST_CASE("invalid moves are rejected") {
  Inventory inv;
  inv.set(0, {kDirt, 10});
  CHECK_FALSE(inv.move(0, 0, 1));                 // same slot
  CHECK_FALSE(inv.move(3, 4, 1));                 // empty source
  CHECK_FALSE(inv.move(-1, 4, 1));                // out of range
  CHECK_FALSE(inv.move(0, kInventorySlots, 1));
  CHECK(inv.slot(0) == ItemStack{kDirt, 10});
}

TEST_CASE("set normalizes empty stacks") {
  Inventory inv;
  inv.set(0, {kDirt, 0});
  CHECK(inv.slot(0) == ItemStack{});
}
