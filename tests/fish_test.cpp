#include "doctest/doctest.h"
#include "item/item.hpp"
#include "item/recipes.hpp"
#include "item/smelting.hpp"
#include "item/tools.hpp"

// ─── Items ─────────────────────────────────────────────────────────────

TEST_CASE("the fishing rod is one to a stack and lasts 64 catches") {
  CHECK(itemDef(ItemId::FISHING_ROD).max_stack == 1);
  CHECK(usesOf(ItemId::FISHING_ROD) == 64);
  // Not a mining tool: hand speed, and it hits like a hand.
  CHECK(toolOf(ItemId::FISHING_ROD).kind == ToolKind::None);
  CHECK(attackDamage(ItemId::FISHING_ROD) == 1);
}

TEST_CASE("usesOf covers tools and nothing else") {
  CHECK(usesOf(ItemId::DIAMOND_PICKAXE) == maxUses(Tier::Diamond));
  CHECK(usesOf(ItemId::WOOD_SWORD) == maxUses(Tier::Wood));
  CHECK(usesOf(ItemId::NONE) == 0);
  CHECK(usesOf(ItemId::COAL) == 0);
  CHECK(usesOf(ItemId::SOGGY_BOOT) == 0);
}

TEST_CASE("raw fish heal a little, cooked fish more, and the boot isn't food") {
  CHECK(itemDef(ItemId::RAW_PEBBLEFISH).food_heal == 2);
  CHECK(itemDef(ItemId::RAW_SUNFIN).food_heal == 2);
  CHECK(itemDef(ItemId::RAW_GLOWFIN).food_heal == 2);
  CHECK(itemDef(ItemId::COOKED_PEBBLEFISH).food_heal == 6);
  CHECK(itemDef(ItemId::COOKED_SUNFIN).food_heal == 6);
  CHECK(itemDef(ItemId::COOKED_GLOWFIN).food_heal == 8);
  CHECK(itemDef(ItemId::SOGGY_BOOT).food_heal == 0);
  CHECK(itemDef(ItemId::SOGGY_BOOT).max_stack == 1);
}

TEST_CASE("the rod is 3 sticks and 2 wool at a table") {
  int found = 0;
  for (const Recipe &r : recipes()) {
    if (r.makes.id != ItemId::FISHING_ROD) continue;
    ++found;
    CHECK(r.needs_table);
    CHECK(r.makes.count == 1);
    REQUIRE(r.needs.size() == 2);
    CHECK(r.needs[0].any_of == std::vector<ItemId>{ItemId::STICKS});
    CHECK(r.needs[0].count == 3);
    CHECK(r.needs[1].any_of == std::vector<ItemId>{ItemId::WOOL});
    CHECK(r.needs[1].count == 2);
  }
  CHECK(found == 1);
}

TEST_CASE("each raw fish cooks into its cooked fish, and cooked fish don't cook again") {
  CHECK(smeltResult(ItemId::RAW_PEBBLEFISH) == ItemId::COOKED_PEBBLEFISH);
  CHECK(smeltResult(ItemId::RAW_SUNFIN) == ItemId::COOKED_SUNFIN);
  CHECK(smeltResult(ItemId::RAW_GLOWFIN) == ItemId::COOKED_GLOWFIN);
  for (ItemId id : {ItemId::COOKED_PEBBLEFISH, ItemId::COOKED_SUNFIN, ItemId::COOKED_GLOWFIN,
                    ItemId::SOGGY_BOOT, ItemId::FISHING_ROD})
    CHECK_FALSE(smeltResult(id));
}
