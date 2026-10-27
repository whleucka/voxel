#include "doctest/doctest.h"
#include "block/block_type.hpp"
#include "item/recipes.hpp"
#include "item/tools.hpp"
#include <set>

namespace {
const ToolKind kKinds[] = {ToolKind::Pickaxe, ToolKind::Axe, ToolKind::Shovel, ToolKind::Sword};
const Tier kTiers[] = {Tier::Wood,  Tier::Stone,   Tier::Copper, Tier::Iron,
                       Tier::Ruby, Tier::Emerald, Tier::Diamond};
} // namespace

TEST_CASE("every kind comes in every tier, one to a stack, and round-trips") {
  std::set<ItemId> seen;
  for (ToolKind k : kKinds)
    for (Tier t : kTiers) {
      ItemId id = toolItem(k, t);
      CAPTURE(itemDef(id).name);
      REQUIRE(isValidItem(id));
      CHECK(itemDef(id).max_stack == 1);
      CHECK(toolOf(id).kind == k);
      CHECK(toolOf(id).tier == t);
      CHECK(maxUses(t) > 0);
      seen.insert(id);
    }
  CHECK(seen.size() == 28);
  CHECK(toolOf(ItemId::NONE).kind == ToolKind::None);
  CHECK(toolOf(ItemId::COAL).kind == ToolKind::None);
  CHECK(toolOf(itemForBlock(BlockType::STONE)).kind == ToolKind::None);
}

TEST_CASE("tool ids are stable") {
  CHECK(static_cast<int>(ItemId::WOOD_PICKAXE) == 272);
  CHECK(static_cast<int>(ItemId::DIAMOND_PICKAXE) == 278);
  CHECK(static_cast<int>(ItemId::WOOD_AXE) == 279);
  CHECK(static_cast<int>(ItemId::DIAMOND_SWORD) == 299);
  CHECK(itemDef(ItemId::STONE_SHOVEL).name == "Stone Shovel");
  CHECK(itemDef(ItemId::WOOD_SWORD).name == "Wooden Sword");
}

TEST_CASE("better tiers mine faster, last longer and hit harder") {
  for (size_t i = 1; i < std::size(kTiers); ++i) {
    CHECK(miningSpeed(kTiers[i]) >= miningSpeed(kTiers[i - 1]));
    CHECK(maxUses(kTiers[i]) >= maxUses(kTiers[i - 1]));
    CHECK(swordDamage(kTiers[i]) >= swordDamage(kTiers[i - 1]));
  }
  CHECK(miningSpeed(Tier::Hand) == 1);
  CHECK(miningSpeed(Tier::Diamond) == 12);
  CHECK(maxUses(Tier::Diamond) == 1600);
}

TEST_CASE("what a hit does with what's held") {
  CHECK(attackDamage(ItemId::NONE) == 1);
  CHECK(attackDamage(itemForBlock(BlockType::DIRT)) == 1);
  CHECK(attackDamage(ItemId::WOOD_SWORD) == 3);
  CHECK(attackDamage(ItemId::DIAMOND_SWORD) == 8);
  CHECK(attackDamage(ItemId::IRON_AXE) == 5);
  CHECK(attackDamage(ItemId::DIAMOND_PICKAXE) == 2);
  CHECK(attackDamage(ItemId::RUBY_SHOVEL) == 2);
}

TEST_CASE("each tool is made from its tier's material and sticks, at a table") {
  int found = 0;
  for (const Recipe &r : recipes()) {
    Tool t = toolOf(r.makes.id);
    if (t.kind == ToolKind::None) continue;
    ++found;
    CAPTURE(itemDef(r.makes.id).name);
    CHECK(r.needs_table);
    CHECK(r.makes.count == 1);
    REQUIRE(r.needs.size() == 2);
    CHECK(r.needs[1].any_of == std::vector<ItemId>{ItemId::STICKS});
    uint16_t head = t.kind == ToolKind::Shovel ? 1 : t.kind == ToolKind::Sword ? 2 : 3;
    CHECK(r.needs[0].count == head);
    if (t.tier == Tier::Iron) CHECK(r.needs[0].any_of == std::vector<ItemId>{ItemId::IRON_INGOT});
    if (t.tier == Tier::Wood) CHECK(r.needs[0].any_of.size() == 4); // any planks
  }
  CHECK(found == 28);
}
