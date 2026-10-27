#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "item/recipes.hpp"

namespace {

ItemId block(BlockType b) { return itemForBlock(b); }

const Recipe &recipeMaking(ItemId id) {
  for (const Recipe &r : recipes())
    if (r.makes.id == id) return r;
  FAIL("no recipe makes that");
  return recipes().front();
}

int count(const Inventory &inv, ItemId id) {
  int n = 0;
  for (const ItemStack &s : inv.slots())
    if (s.id == id) n += s.count;
  return n;
}

} // namespace

TEST_CASE("every recipe is made of real items in sensible amounts") {
  REQUIRE_FALSE(recipes().empty());
  for (const Recipe &r : recipes()) {
    CAPTURE(itemDef(r.makes.id).name);
    CHECK(isValidItem(r.makes.id));
    CHECK(r.makes.count > 0);
    CHECK(r.makes.count <= itemDef(r.makes.id).max_stack);
    REQUIRE_FALSE(r.needs.empty());
    for (const Ingredient &in : r.needs) {
      CHECK(in.count > 0);
      REQUIRE_FALSE(in.any_of.empty());
      for (ItemId id : in.any_of) {
        CHECK(isValidItem(id));
        CHECK(in.count <= itemDef(id).max_stack);
      }
    }
  }
}

TEST_CASE("recipe ids are stable: they travel on the wire") {
  const auto &all = recipes();
  REQUIRE(all.size() >= 17);
  CHECK(all[0].makes.id == block(BlockType::OAK_PLANKS));
  CHECK(all[4].makes.id == ItemId::STICKS);
  CHECK(all[5].makes.id == block(BlockType::CRAFTING_TABLE));
  CHECK(all[6].makes.id == block(BlockType::STONE_BRICKS));
  CHECK(all[8].makes.id == block(BlockType::RED_BLOCK));
  CHECK(all[15].makes.id == block(BlockType::GLOWSTONE));
  CHECK(all[16].makes.id == block(BlockType::FURNACE));
}

TEST_CASE("only planks, sticks and the table can be made without a table") {
  for (const Recipe &r : recipes()) {
    CAPTURE(itemDef(r.makes.id).name);
    bool basic = r.makes.id == ItemId::STICKS ||
                 r.makes.id == block(BlockType::CRAFTING_TABLE) ||
                 r.makes.id == block(BlockType::OAK_PLANKS) ||
                 r.makes.id == block(BlockType::PINE_PLANKS) ||
                 r.makes.id == block(BlockType::PALM_PLANKS) ||
                 r.makes.id == block(BlockType::CHERRY_PLANKS);
    CHECK(r.needs_table == !basic);
  }
}

TEST_CASE("a recipe is known once every ingredient has been seen") {
  const Recipe &red = recipeMaking(block(BlockType::RED_BLOCK));
  SeenItems seen;
  CHECK_FALSE(knows(red, seen));
  see(seen, block(BlockType::WHITE_BLOCK));
  CHECK_FALSE(knows(red, seen));
  see(seen, ItemId::RUBY);
  CHECK(knows(red, seen));

  // "Any planks": seeing one kind is enough.
  SeenItems palm;
  see(palm, block(BlockType::PALM_PLANKS));
  CHECK(knows(recipeMaking(ItemId::STICKS), palm));
}

TEST_CASE("crafting uses up the ingredients and adds the output") {
  Inventory inv;
  inv.set(0, {block(BlockType::OAK_LOG), 3});
  auto after = craftOnce(recipeMaking(block(BlockType::OAK_PLANKS)), inv);
  REQUIRE(after);
  CHECK(count(*after, block(BlockType::OAK_LOG)) == 2);
  CHECK(count(*after, block(BlockType::OAK_PLANKS)) == 4);
}

TEST_CASE("any planks may be mixed") {
  Inventory inv;
  inv.set(0, {block(BlockType::OAK_PLANKS), 1});
  inv.set(1, {block(BlockType::CHERRY_PLANKS), 3});
  const Recipe &table = recipeMaking(block(BlockType::CRAFTING_TABLE));
  CHECK(timesAffordable(table, inv) == 1);
  auto after = craftOnce(table, inv);
  REQUIRE(after);
  CHECK(count(*after, block(BlockType::OAK_PLANKS)) == 0);
  CHECK(count(*after, block(BlockType::CHERRY_PLANKS)) == 0);
  CHECK(count(*after, block(BlockType::CRAFTING_TABLE)) == 1);
}

TEST_CASE("short ingredients or no room means no craft") {
  const Recipe &white = recipeMaking(block(BlockType::WHITE_BLOCK));
  Inventory inv;
  inv.set(0, {block(BlockType::SAND), 7});
  inv.set(1, {ItemId::WOOL, 1});
  CHECK(timesAffordable(white, inv) == 0);
  CHECK_FALSE(craftOnce(white, inv));

  // Enough, but every slot is full of something else that can't take it.
  Inventory full;
  for (int i = 0; i < kInventorySlots; ++i) full.set(i, {block(BlockType::DIRT), 64});
  full.set(0, {block(BlockType::SAND), 64});
  full.set(1, {ItemId::WOOL, 64});
  CHECK(timesAffordable(white, full) == 8);
  CHECK_FALSE(craftOnce(white, full));
}

TEST_CASE("a used-up ingredient's slot can take the output") {
  Inventory full;
  for (int i = 0; i < kInventorySlots; ++i) full.set(i, {block(BlockType::DIRT), 64});
  full.set(5, {block(BlockType::OAK_LOG), 1});
  auto after = craftOnce(recipeMaking(block(BlockType::OAK_PLANKS)), full);
  REQUIRE(after);
  CHECK(count(*after, block(BlockType::OAK_PLANKS)) == 4);
}

TEST_CASE("storage is used before the hotbar") {
  Inventory inv;
  inv.set(0, {block(BlockType::COBBLESTONE), 4});            // in hand
  inv.set(kHotbarSlots, {block(BlockType::COBBLESTONE), 4}); // in storage
  auto after = craftOnce(recipeMaking(block(BlockType::STONE_BRICKS)), inv);
  REQUIRE(after);
  CHECK(after->slot(0) == ItemStack{block(BlockType::COBBLESTONE), 4});
}
