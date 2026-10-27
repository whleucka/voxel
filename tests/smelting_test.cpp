#include "doctest/doctest.h"
#include "block/block_type.hpp"
#include "item/smelting.hpp"

namespace {

ItemId block(BlockType b) { return itemForBlock(b); }

Furnace loaded(ItemStack input, ItemStack fuel) {
  Furnace f;
  f.input = input;
  f.fuel = fuel;
  return f;
}

// Runs in game ticks, as the Game does.
FurnaceStep run(Furnace &f, int ms) {
  FurnaceStep all;
  for (int t = 0; t < ms; t += 50) {
    FurnaceStep s = tickFurnace(f, 50);
    all.lit_changed |= s.lit_changed;
    all.slots_changed |= s.slots_changed;
  }
  return all;
}

} // namespace

TEST_CASE("what smelts into what, and what burns") {
  CHECK(smeltResult(ItemId::RAW_IRON) == ItemId::IRON_INGOT);
  CHECK(smeltResult(ItemId::RAW_COPPER) == ItemId::COPPER_INGOT);
  CHECK(smeltResult(ItemId::RAW_GOLD) == ItemId::GOLD_INGOT);
  CHECK(smeltResult(block(BlockType::SAND)) == block(BlockType::GLASS));
  CHECK(smeltResult(block(BlockType::COBBLESTONE)) == block(BlockType::STONE));
  CHECK(smeltResult(block(BlockType::SANDSTONE)) == block(BlockType::BRICKS));
  for (BlockType log : {BlockType::OAK_LOG, BlockType::PINE_LOG, BlockType::PALM_LOG,
                        BlockType::CHERRY_LOG}) {
    CHECK(smeltResult(block(log)) == ItemId::CHARCOAL);
    CHECK(fuelMs(block(log)) == 2 * kSmeltMs);
  }
  // Results don't smelt again, and plain things don't smelt or burn.
  for (ItemId id : {ItemId::IRON_INGOT, ItemId::CHARCOAL, block(BlockType::GLASS),
                    block(BlockType::STONE), block(BlockType::DIRT)})
    CHECK_FALSE(smeltResult(id));
  CHECK(fuelMs(ItemId::COAL) == 8 * kSmeltMs);
  CHECK(fuelMs(ItemId::CHARCOAL) == 8 * kSmeltMs);
  CHECK(fuelMs(ItemId::STICKS) == kSmeltMs / 2);
  CHECK(fuelMs(block(BlockType::DIRT)) == 0);
}

TEST_CASE("a furnace lights with something to cook and fuel, then cooks one per 5 s") {
  Furnace f = loaded({ItemId::RAW_IRON, 3}, {ItemId::COAL, 1});
  FurnaceStep s = tickFurnace(f, 50);
  CHECK(s.lit_changed);
  CHECK(f.lit());
  CHECK(f.fuel.empty());
  run(f, kSmeltMs - 50);
  CHECK(f.output == ItemStack{ItemId::IRON_INGOT, 1});
  CHECK(f.input.count == 2);
  run(f, 2 * kSmeltMs);
  CHECK(f.output.count == 3);
  CHECK(f.input.empty());
  CHECK(f.lit()); // the coal burns on with nothing left to cook
}

TEST_CASE("no fuel, or nothing that smelts, means no fire") {
  Furnace no_fuel = loaded({ItemId::RAW_IRON, 1}, {});
  CHECK_FALSE(tickFurnace(no_fuel, 1000).lit_changed);
  Furnace no_input = loaded({}, {ItemId::COAL, 1});
  tickFurnace(no_input, 1000);
  CHECK_FALSE(no_input.lit());
  CHECK(no_input.fuel.count == 1);
  Furnace dirt = loaded({block(BlockType::DIRT), 1}, {ItemId::COAL, 1});
  tickFurnace(dirt, 1000);
  CHECK_FALSE(dirt.lit());
  Furnace not_fuel = loaded({ItemId::RAW_IRON, 1}, {block(BlockType::DIRT), 5});
  tickFurnace(not_fuel, 1000);
  CHECK_FALSE(not_fuel.lit());
}

TEST_CASE("a lump of coal smelts eight, then the fire goes out") {
  Furnace f = loaded({ItemId::RAW_COPPER, 20}, {ItemId::COAL, 1});
  run(f, 9 * kSmeltMs);
  CHECK(f.output.count == 8);
  CHECK(f.input.count == 12);
  CHECK_FALSE(f.lit());
}

TEST_CASE("half fuels add up: four sticks smelt two") {
  Furnace f = loaded({block(BlockType::SAND), 5}, {ItemId::STICKS, 4});
  run(f, 4 * kSmeltMs);
  CHECK(f.output == ItemStack{block(BlockType::GLASS), 2});
  CHECK(f.fuel.empty());
}

TEST_CASE("a full or different output pauses cooking but the fire burns down") {
  Furnace full = loaded({ItemId::RAW_GOLD, 2}, {ItemId::COAL, 2});
  full.output = {ItemId::GOLD_INGOT, 64};
  run(full, 2 * kSmeltMs);
  CHECK_FALSE(full.lit()); // never lit: it couldn't cook
  CHECK(full.fuel.count == 2);

  Furnace burning = loaded({ItemId::RAW_GOLD, 1}, {ItemId::COAL, 1});
  run(burning, 1000); // lit, partway
  burning.output = {ItemId::IRON_INGOT, 1}; // someone put something else there
  int cooked = burning.cook_ms;
  run(burning, kSmeltMs);
  CHECK(burning.cook_ms == cooked); // paused, progress kept
  CHECK(burning.input.count == 1);
  CHECK(burning.burn_ms < 8 * kSmeltMs - 1000);
}

TEST_CASE("one big step equals many small ones") {
  Furnace a = loaded({ItemId::RAW_IRON, 10}, {ItemId::STICKS, 7});
  Furnace b = a;
  tickFurnace(a, 6 * kSmeltMs);
  run(b, 6 * kSmeltMs);
  CHECK(a == b);
}
