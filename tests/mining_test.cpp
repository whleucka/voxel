#include "doctest/doctest.h"
#include "item/mining.hpp"

TEST_CASE("bare hands: slow, and stone and ore give nothing") {
  CHECK(breakMs(BlockType::DIRT, ItemId::NONE) == 750);
  CHECK(breakMs(BlockType::OAK_LOG, ItemId::NONE) == 3000);
  CHECK(breakMs(BlockType::STONE, ItemId::NONE) == 7500);
  CHECK(breakMs(BlockType::DIAMOND_ORE, ItemId::NONE) == 12000);
  CHECK(dropsWith(BlockType::DIRT, ItemId::NONE));
  CHECK(dropsWith(BlockType::OAK_LOG, ItemId::NONE)); // you can always get started
  CHECK_FALSE(dropsWith(BlockType::STONE, ItemId::NONE));
  CHECK_FALSE(dropsWith(BlockType::COAL_ORE, ItemId::NONE));
}

TEST_CASE("the right tool divides the time by its speed; the wrong one doesn't help") {
  CHECK(breakMs(BlockType::STONE, ItemId::WOOD_PICKAXE) == 3750);
  CHECK(breakMs(BlockType::STONE, ItemId::DIAMOND_PICKAXE) == 625);
  CHECK(breakMs(BlockType::OAK_LOG, ItemId::STONE_AXE) == 750);
  CHECK(breakMs(BlockType::SAND, ItemId::IRON_SHOVEL) == 93);
  CHECK(breakMs(BlockType::STONE, ItemId::DIAMOND_SHOVEL) == 7500);
  CHECK(breakMs(BlockType::OAK_LOG, ItemId::DIAMOND_PICKAXE) == 3000);
  CHECK(breakMs(BlockType::OAK_LEAF, ItemId::WOOD_SWORD) == 150); // swords cut leaves
  CHECK(breakMs(BlockType::STONE, itemForBlock(BlockType::DIRT)) == 7500); // a block in hand
}

TEST_CASE("ore gates: each ore needs a good enough pickaxe to drop") {
  struct Gate { BlockType ore; ItemId too_weak; ItemId enough; };
  const Gate gates[] = {
      {BlockType::STONE, ItemId::NONE, ItemId::WOOD_PICKAXE},
      {BlockType::COAL_ORE, ItemId::WOOD_PICKAXE, ItemId::STONE_PICKAXE},
      {BlockType::COPPER_ORE, ItemId::WOOD_PICKAXE, ItemId::STONE_PICKAXE},
      {BlockType::IRON_ORE, ItemId::STONE_PICKAXE, ItemId::COPPER_PICKAXE},
      {BlockType::GOLD_ORE, ItemId::STONE_PICKAXE, ItemId::COPPER_PICKAXE},
      {BlockType::RUBY_ORE, ItemId::COPPER_PICKAXE, ItemId::IRON_PICKAXE},
      {BlockType::DIAMOND_ORE, ItemId::COPPER_PICKAXE, ItemId::IRON_PICKAXE},
  };
  for (const Gate &g : gates) {
    CAPTURE(static_cast<int>(g.ore));
    CHECK_FALSE(dropsWith(g.ore, g.too_weak));
    CHECK(dropsWith(g.ore, g.enough));
    CHECK(dropsWith(g.ore, ItemId::DIAMOND_PICKAXE));
    CHECK_FALSE(dropsWith(g.ore, ItemId::DIAMOND_AXE)); // only a pickaxe counts
  }
}

TEST_CASE("glass needs a pickaxe to come back; bedrock never breaks") {
  CHECK_FALSE(dropsWith(BlockType::GLASS, ItemId::NONE));
  CHECK(dropsWith(BlockType::GLASS, ItemId::WOOD_PICKAXE));
  CHECK(breakMs(BlockType::BEDROCK, ItemId::DIAMOND_PICKAXE) == -1);
  CHECK(breakMs(BlockType::WATER, ItemId::NONE) == -1);
  CHECK_FALSE(dropsWith(BlockType::BEDROCK, ItemId::DIAMOND_PICKAXE));
}

TEST_CASE("every breakable block breaks in a sensible time") {
  for (uint16_t v = 1; v <= static_cast<uint16_t>(kLastBlockType); ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(v);
    int ms = breakMs(b, ItemId::NONE);
    if (ms < 0) continue;
    CHECK(ms >= 300);
    CHECK(ms <= 12000);
    CHECK(breakMs(b, ItemId::DIAMOND_PICKAXE) <= ms);
  }
}

TEST_CASE("which blocks are ore") {
  CHECK(isOre(BlockType::COAL_ORE));
  CHECK(isOre(BlockType::DIAMOND_ORE));
  CHECK_FALSE(isOre(BlockType::STONE));
  CHECK_FALSE(isOre(BlockType::GLOWSTONE));
}
