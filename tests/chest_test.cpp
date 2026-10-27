#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include "item/chest.hpp"
#include "item/recipes.hpp"
#include <algorithm>
#include <map>
#include <set>

// ─── Blocks and items ──────────────────────────────────────────────────

TEST_CASE("chests are blocks with art; only the plain one is an item") {
  for (BlockType b : {BlockType::CHEST, BlockType::TREASURE_CHEST}) {
    CAPTURE(blockName(b));
    CHECK(blockName(b) != "Unknown");
    CHECK(block_data.count(b) == 1);
  }
  CHECK(static_cast<uint16_t>(BlockType::CHEST) + 1 ==
        static_cast<uint16_t>(BlockType::TREASURE_CHEST));
  CHECK(isValidItem(itemForBlock(BlockType::CHEST)));
  CHECK(dropFor(BlockType::CHEST) == itemForBlock(BlockType::CHEST));
  // Found, never held, placed or picked from the palette.
  CHECK_FALSE(isValidItem(itemForBlock(BlockType::TREASURE_CHEST)));
  CHECK_FALSE(dropFor(BlockType::TREASURE_CHEST));
  const auto &palette = paletteItems();
  CHECK(std::find(palette.begin(), palette.end(), itemForBlock(BlockType::TREASURE_CHEST)) ==
        palette.end());
  CHECK(std::find(palette.begin(), palette.end(), itemForBlock(BlockType::CHEST)) !=
        palette.end());
}

TEST_CASE("a chest mines like planks; a treasure chest never breaks") {
  CHECK(toughness(BlockType::CHEST).breakable);
  CHECK(toughness(BlockType::CHEST).best == ToolKind::Axe);
  CHECK(toughness(BlockType::CHEST).hand_ms == toughness(BlockType::OAK_PLANKS).hand_ms);
  CHECK_FALSE(toughness(BlockType::TREASURE_CHEST).breakable);
}

TEST_CASE("a chest is 8 planks, a key 2 gold ingots and a gem, both at a table") {
  int chests = 0, keys = 0;
  for (const Recipe &r : recipes()) {
    if (r.makes.id == itemForBlock(BlockType::CHEST)) {
      ++chests;
      CHECK(r.needs_table);
      REQUIRE(r.needs.size() == 1);
      CHECK(r.needs[0].count == 8);
      CHECK(r.needs[0].any_of.size() == 4); // any planks
    }
    if (r.makes.id == ItemId::TREASURE_KEY) {
      ++keys;
      CHECK(r.needs_table);
      CHECK(r.makes.count == 1);
      REQUIRE(r.needs.size() == 2);
      CHECK(r.needs[0].any_of == std::vector<ItemId>{ItemId::GOLD_INGOT});
      CHECK(r.needs[0].count == 2);
      CHECK(r.needs[1].any_of ==
            std::vector<ItemId>{ItemId::RUBY, ItemId::EMERALD, ItemId::DIAMOND});
      CHECK(r.needs[1].count == 1);
    }
  }
  CHECK(chests == 1);
  CHECK(keys == 1);
  CHECK(itemDef(ItemId::TREASURE_KEY).max_stack == 16);
}

// ─── Loot ──────────────────────────────────────────────────────────────

TEST_CASE("a treasure chest holds 4-6 different stacks from the loot table") {
  std::mt19937 rng(42);
  std::map<ItemId, int> seen;
  for (int i = 0; i < 2000; ++i) {
    Chest c = rollTreasure(rng);
    std::set<ItemId> ids;
    int stacks = 0;
    for (const ItemStack &s : c) {
      if (s.empty()) continue;
      ++stacks;
      REQUIRE(isValidItem(s.id));
      REQUIRE(s.count <= itemDef(s.id).max_stack);
      REQUIRE(s.wear == 0); // tools come unworn
      ids.insert(s.id);
      ++seen[s.id];
      auto line = std::find_if(treasureLoot().begin(), treasureLoot().end(),
                               [&](const TreasureLoot &l) { return l.id == s.id; });
      REQUIRE(line != treasureLoot().end());
      REQUIRE(s.count >= line->min);
      REQUIRE(s.count <= line->max);
    }
    REQUIRE(stacks >= kTreasureMinStacks);
    REQUIRE(stacks <= kTreasureMaxStacks);
    REQUIRE(ids.size() == static_cast<size_t>(stacks)); // no repeats
  }
  // Roughly by weight: common things often, the rarest now and then.
  CHECK(seen[ItemId::GOLD_INGOT] > seen[ItemId::DIAMOND]);
  CHECK(seen[ItemId::DIAMOND] > seen[ItemId::RUBY_SWORD]);
  CHECK(seen[ItemId::TREASURE_KEY] > 0);
  CHECK(seen[ItemId::RUBY_SWORD] > 0);
}
