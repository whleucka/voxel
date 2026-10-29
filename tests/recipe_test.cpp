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

TEST_CASE("only planks, sticks, the table, a boat, torches and compost need no table") {
  for (const Recipe &r : recipes()) {
    CAPTURE(itemDef(r.makes.id).name);
    bool basic = r.makes.id == ItemId::STICKS ||
                 r.makes.id == block(BlockType::CRAFTING_TABLE) ||
                 r.makes.id == block(BlockType::OAK_PLANKS) ||
                 r.makes.id == block(BlockType::PINE_PLANKS) ||
                 r.makes.id == block(BlockType::PALM_PLANKS) ||
                 r.makes.id == block(BlockType::CHERRY_PLANKS) ||
                 r.makes.id == ItemId::BOAT || // made at the shore
                 r.makes.id == block(BlockType::TORCH) || // made in a dark cave
                 r.makes.id == ItemId::COMPOST;
    CHECK(r.needs_table == !basic);
  }
}

TEST_CASE("a recipe is known once every ingredient has been seen") {
  const Recipe &red = recipeMaking(block(BlockType::RED_BLOCK));
  SeenItems seen;
  CHECK_FALSE(knows(red, seen));
  see(seen, block(BlockType::WHITE_BLOCK));
  CHECK_FALSE(knows(red, seen));
  see(seen, block(BlockType::POPPY));
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

// ─── Biome plants ──────────────────────────────────────────────────────

namespace {

// Whether some recipe makes `makes` from `needs` (as the only choice, or
// one of them, for each ingredient), and how many.
std::optional<uint16_t> made(ItemId makes, std::vector<std::pair<ItemId, uint16_t>> needs) {
  for (const Recipe &r : recipes()) {
    if (r.makes.id != makes || r.needs.size() != needs.size()) continue;
    bool all = true;
    for (size_t i = 0; i < needs.size(); ++i) {
      const auto &any = r.needs[i].any_of;
      all &= r.needs[i].count == needs[i].second &&
             std::find(any.begin(), any.end(), needs[i].first) != any.end();
    }
    if (all) return r.makes.count;
  }
  return std::nullopt;
}

bool usedBySomething(ItemId id) {
  for (const Recipe &r : recipes())
    for (const Ingredient &in : r.needs)
      if (std::find(in.any_of.begin(), in.any_of.end(), id) != in.any_of.end()) return true;
  return false;
}

} // namespace

TEST_CASE("each colour's dye is its flower, and gems no longer paint") {
  const ItemId white = block(BlockType::WHITE_BLOCK);
  const std::pair<BlockType, BlockType> colours[] = {
      {BlockType::RED_BLOCK, BlockType::POPPY},     {BlockType::ORANGE_BLOCK, BlockType::HIBISCUS},
      {BlockType::YELLOW_BLOCK, BlockType::BUTTERCUP}, {BlockType::GREEN_BLOCK, BlockType::CACTUS},
      {BlockType::BLUE_BLOCK, BlockType::CORNFLOWER}, {BlockType::PURPLE_BLOCK, BlockType::LAVENDER},
      {BlockType::PINK_BLOCK, BlockType::PINK_TULIP}};
  for (const auto &[colour, plant] : colours) {
    CAPTURE(blockName(colour));
    CHECK(made(block(colour), {{white, 8}, {block(plant), 1}}) == 8);
    CHECK(made(bedItem(static_cast<BedColour>(static_cast<int>(colour) -
                                              static_cast<int>(BlockType::WHITE_BLOCK))),
               {{ItemId::WHITE_BED, 1}, {block(plant), 1}}) == 1);
  }
  for (ItemId old : {ItemId::RUBY, ItemId::RAW_COPPER, ItemId::RAW_GOLD, ItemId::EMERALD,
                     ItemId::BERRIES, ItemId::SWEET_PETALS})
    for (const auto &[colour, plant] : colours) {
      CAPTURE(itemDef(old).name);
      CHECK_FALSE(made(block(colour), {{white, 8}, {old, 1}}));
    }
  // The red recipe keeps its id: it's the same recipe with a new dye.
  CHECK(recipes()[8].makes.id == block(BlockType::RED_BLOCK));
}

TEST_CASE("white blocks come from sand and wool, a daisy or a sea shell") {
  const ItemId sand = block(BlockType::SAND), white = block(BlockType::WHITE_BLOCK);
  CHECK(made(white, {{sand, 8}, {ItemId::WOOL, 1}}) == 8);
  CHECK(made(white, {{sand, 8}, {block(BlockType::DAISY), 1}}) == 8);
  CHECK(made(white, {{sand, 8}, {ItemId::SEA_SHELL, 1}}) == 8);
  CHECK(recipes()[7].makes.id == white);
}

TEST_CASE("torches, bricks, compost and the plant foods") {
  CHECK(made(block(BlockType::TORCH), {{ItemId::COAL, 1}, {ItemId::STICKS, 1}}) == 4);
  CHECK(made(block(BlockType::TORCH), {{ItemId::CHARCOAL, 1}, {ItemId::STICKS, 1}}) == 4);
  CHECK(made(block(BlockType::SANDSTONE_BRICKS), {{block(BlockType::SANDSTONE), 4}}) == 4);
  CHECK(made(block(BlockType::SNOW_BRICKS), {{block(BlockType::SNOW), 4}}) == 4);
  for (BlockType leaf : {BlockType::OAK_LEAF, BlockType::PINE_LEAF, BlockType::PALM_LEAF,
                         BlockType::CHERRY_LEAF})
    CHECK(made(ItemId::COMPOST, {{block(leaf), 4}}) == 1);
  CHECK(made(ItemId::MUSHROOM_STEW, {{block(BlockType::RED_MUSHROOM), 1},
                                     {block(BlockType::BROWN_MUSHROOM), 1}}) == 1);
  CHECK(made(ItemId::BERRY_COOKIE, {{ItemId::WHEAT, 2}, {ItemId::BERRIES, 1}}) == 4);
  CHECK(made(block(BlockType::CAKE_4), {{ItemId::WHEAT, 3},
                                        {ItemId::STRAWBERRY, 2},
                                        {ItemId::SWEET_PETALS, 1}}) == 1);
  CHECK(itemDef(ItemId::MUSHROOM_STEW).food_heal == 8);
  CHECK(itemDef(ItemId::BERRY_COOKIE).food_heal == 2);
}

TEST_CASE("nothing new is a dead end: every plant and find is used for something") {
  // In a recipe...
  for (BlockType b : {BlockType::POPPY, BlockType::BUTTERCUP, BlockType::CORNFLOWER,
                      BlockType::PINK_TULIP, BlockType::DAISY, BlockType::LAVENDER,
                      BlockType::HIBISCUS, BlockType::RED_MUSHROOM, BlockType::BROWN_MUSHROOM,
                      BlockType::CACTUS, BlockType::SANDSTONE, BlockType::SNOW,
                      BlockType::OAK_LEAF, BlockType::PINE_LEAF})
    CHECK(usedBySomething(block(b)));
  for (ItemId id : {ItemId::SEA_SHELL, ItemId::BERRIES, ItemId::SWEET_PETALS, ItemId::RUBY,
                    ItemId::EMERALD, ItemId::STICKS, ItemId::COAL, ItemId::WHEAT})
    CHECK(usedBySomething(id));
  // ...or useful in itself: placed, eaten, planted or used. (Ferns feed
  // Blossom Deer, a berry bush is replanted, coral and torches are placed,
  // dead bushes give sticks and tall grass seeds.)
  CHECK(itemDef(ItemId::MUSHROOM_STEW).food_heal > 0);
  CHECK(itemDef(ItemId::BERRY_COOKIE).food_heal > 0);
}

// ─── Stairs and slabs ──────────────────────────────────────────────────

TEST_CASE("6 of a material make 4 stairs, and 3 make 6 slabs, at a table") {
  for (int m = 0; m < kShapeMaterials; ++m) {
    const ItemId material = block(kShapeMaterial[m]);
    const ItemId stairs = block(stairBlock(m, DoorFacing::North)), slab = block(slabBlock(m));
    CAPTURE(itemDef(material).name);
    CHECK(made(stairs, {{material, 6}}) == 4);
    CHECK(made(slab, {{material, 3}}) == 6);
    for (ItemId id : {stairs, slab}) {
      const Recipe &r = recipeMaking(id);
      CHECK(r.needs_table);
      CHECK(r.needs.size() == 1);
      CHECK(r.needs[0].any_of.size() == 1); // oak stairs from oak planks only
      // Learned once you've held the material.
      SeenItems seen;
      CHECK_FALSE(knows(r, seen));
      seen.set(static_cast<size_t>(material));
      CHECK(knows(r, seen));
    }
  }
}
