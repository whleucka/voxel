#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "item/item.hpp"
#include "item/item_stack.hpp"
#include "item/tools.hpp"
#include <set>
#include <utility>

namespace {

constexpr uint16_t kLastBlock = static_cast<uint16_t>(kLastBlockType);

std::vector<ItemId> materials() {
  std::vector<ItemId> out;
  for (uint16_t v = static_cast<uint16_t>(kFirstMaterialItem);
       v <= static_cast<uint16_t>(kLastMaterialItem); ++v)
    out.push_back(static_cast<ItemId>(v));
  return out;
}

} // namespace

// ─── Definitions ───────────────────────────────────────────────────────

TEST_CASE("kLastBlockType is the last named block type") {
  CHECK(blockName(kLastBlockType) != "Unknown");
  CHECK(blockName(static_cast<BlockType>(kLastBlock + 1)) == "Unknown");
}

TEST_CASE("every placeable block has an item that places it") {
  for (uint16_t v = 1; v <= kLastBlock; ++v) {
    BlockType b = static_cast<BlockType>(v);
    if (b == BlockType::WATER || b == BlockType::FURNACE_LIT ||
        b == BlockType::TREASURE_CHEST || isDoor(b) || isBed(b) || isFarmland(b) || isCrop(b) ||
        b == BlockType::BERRY_BUSH_BARE || b == BlockType::SHELLS ||
        (isTorch(b) && b != BlockType::TORCH) || (isCake(b) && b != BlockType::CAKE_4))
      continue;
    ItemId id = itemForBlock(b);
    CAPTURE(blockName(b));
    REQUIRE(isValidItem(id));
    const ItemDef &def = itemDef(id);
    CHECK(def.places == b);
    CHECK(def.name == blockName(b));
    CHECK(def.max_stack == 64);
    CHECK(def.icon.atlas == ItemIcon::Atlas::Block);
    CHECK(def.icon.tile == block_data.at(b).side);
  }
}

TEST_CASE("the material items are defined with unique item-atlas tiles") {
  auto mats = materials();
  CHECK(mats.size() == 78);

  std::set<std::pair<int, int>> tiles;
  for (ItemId id : mats) {
    REQUIRE(isValidItem(id));
    const ItemDef &def = itemDef(id);
    CAPTURE(def.name);
    CHECK(!def.name.empty());
    CHECK(def.name != "Unknown");
    bool single = usesOf(id) > 0 || id == ItemId::SOGGY_BOOT || id == ItemId::BOAT;
    const bool sixteen =
        id == ItemId::TREASURE_KEY || id == ItemId::WOODEN_DOOR || isBedItem(id);
    CHECK(def.max_stack == (single ? 1 : sixteen ? 16 : 64));
    CHECK_FALSE(def.places.has_value());
    CHECK(def.icon.atlas == ItemIcon::Atlas::Item);
    CHECK(def.icon.tile.x >= 0);
    CHECK(def.icon.tile.x < 8);
    CHECK(def.icon.tile.y >= 0);
    CHECK(def.icon.tile.y < 16);
    tiles.insert({def.icon.tile.x, def.icon.tile.y});
  }
  CHECK(tiles.size() == mats.size());
}

TEST_CASE("material item ids are stable") {
  // Persisted in saves: these numbers must never change.
  CHECK(static_cast<uint16_t>(ItemId::COAL) == 256);
  CHECK(static_cast<uint16_t>(ItemId::RAW_COPPER) == 262);
  CHECK(static_cast<uint16_t>(ItemId::WOOL) == 263);
  CHECK(static_cast<uint16_t>(ItemId::BERRIES) == 264);
  CHECK(static_cast<uint16_t>(ItemId::SEA_SHELL) == 265);
  CHECK(static_cast<uint16_t>(ItemId::SWEET_PETALS) == 266);
  CHECK(static_cast<uint16_t>(ItemId::STICKS) == 267);
  CHECK(static_cast<uint16_t>(ItemId::IRON_INGOT) == 268);
  CHECK(static_cast<uint16_t>(ItemId::CHARCOAL) == 271);
  CHECK(static_cast<uint16_t>(ItemId::DIAMOND_SWORD) == 299);
  CHECK(static_cast<uint16_t>(ItemId::FISHING_ROD) == 300);
  CHECK(static_cast<uint16_t>(ItemId::COOKED_GLOWFIN) == 306);
  CHECK(static_cast<uint16_t>(ItemId::SOGGY_BOOT) == 307);
  CHECK(static_cast<uint16_t>(ItemId::TREASURE_KEY) == 308);
  CHECK(static_cast<uint16_t>(ItemId::WOODEN_DOOR) == 309);
  CHECK(static_cast<uint16_t>(ItemId::WHITE_BED) == 310);
  CHECK(static_cast<uint16_t>(ItemId::PINK_BED) == 317);
}

TEST_CASE("isValidItem rejects the empty item, water and unknown ids") {
  CHECK_FALSE(isValidItem(ItemId::NONE));
  CHECK_FALSE(isValidItem(itemForBlock(BlockType::WATER)));
  CHECK_FALSE(isValidItem(static_cast<ItemId>(kLastBlock + 1)));
  CHECK_FALSE(isValidItem(static_cast<ItemId>(255)));
  CHECK_FALSE(isValidItem(
      static_cast<ItemId>(static_cast<uint16_t>(kLastMaterialItem) + 1)));
  CHECK(itemDef(static_cast<ItemId>(9999)).name == "Unknown");
}

// ─── Drops ─────────────────────────────────────────────────────────────

TEST_CASE("air, water and bedrock drop nothing") {
  CHECK_FALSE(dropFor(BlockType::AIR).has_value());
  CHECK_FALSE(dropFor(BlockType::WATER).has_value());
  CHECK_FALSE(dropFor(BlockType::BEDROCK).has_value());
}

TEST_CASE("grass and snow dirt drop dirt; stone drops cobblestone") {
  CHECK(dropFor(BlockType::GRASS) == itemForBlock(BlockType::DIRT));
  CHECK(dropFor(BlockType::SNOW_DIRT) == itemForBlock(BlockType::DIRT));
  CHECK(dropFor(BlockType::STONE) == itemForBlock(BlockType::COBBLESTONE));
}

TEST_CASE("ores drop their material") {
  CHECK(dropFor(BlockType::COAL_ORE) == ItemId::COAL);
  CHECK(dropFor(BlockType::DIAMOND_ORE) == ItemId::DIAMOND);
  CHECK(dropFor(BlockType::EMERALD_ORE) == ItemId::EMERALD);
  CHECK(dropFor(BlockType::RUBY_ORE) == ItemId::RUBY);
  CHECK(dropFor(BlockType::IRON_ORE) == ItemId::RAW_IRON);
  CHECK(dropFor(BlockType::GOLD_ORE) == ItemId::RAW_GOLD);
  CHECK(dropFor(BlockType::COPPER_ORE) == ItemId::RAW_COPPER);
}

TEST_CASE("every other block drops itself, and every drop is a valid item") {
  const std::set<BlockType> special = {
      BlockType::AIR,         BlockType::WATER,       BlockType::BEDROCK,
      BlockType::GRASS,       BlockType::SNOW_DIRT,   BlockType::STONE,
      BlockType::COAL_ORE,    BlockType::DIAMOND_ORE, BlockType::EMERALD_ORE,
      BlockType::RUBY_ORE,    BlockType::IRON_ORE,    BlockType::GOLD_ORE,
      BlockType::COPPER_ORE,  BlockType::FURNACE_LIT, BlockType::TREASURE_CHEST,
      BlockType::FARMLAND_DRY, BlockType::FARMLAND_WET, BlockType::TALL_GRASS,
      BlockType::DEAD_BUSH,   BlockType::BERRY_BUSH_BARE, BlockType::SHELLS,
  };
  for (uint16_t v = 0; v <= kLastBlock; ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(blockName(b));
    auto drop = dropFor(b);
    if (drop) CHECK(isValidItem(*drop));
    if (!special.count(b) && !isDoor(b) && !isBed(b) && !isCrop(b) && !isTorch(b) && !isCake(b))
      CHECK(drop == itemForBlock(b));
  }
}

// ─── Stacks ────────────────────────────────────────────────────────────

namespace {
const ItemId kDirt = itemForBlock(BlockType::DIRT);
const ItemId kSand = itemForBlock(BlockType::SAND);
} // namespace

TEST_CASE("empty stacks") {
  CHECK(ItemStack{}.empty());
  CHECK(ItemStack{kDirt, 0}.empty());
  CHECK(ItemStack{ItemId::NONE, 5}.empty());
  CHECK_FALSE(ItemStack{kDirt, 1}.empty());
  CHECK(ItemStack{}.space() == 64);
  CHECK(ItemStack{kDirt, 60}.space() == 4);
  CHECK(ItemStack{kDirt, 64}.space() == 0);
}

TEST_CASE("merging into an empty stack adopts the source") {
  ItemStack dst, src{kDirt, 10};
  CHECK(mergeInto(dst, src) == 10);
  CHECK(dst == ItemStack{kDirt, 10});
  CHECK(src == ItemStack{});
}

TEST_CASE("merging into a partial stack fills it and keeps the remainder") {
  ItemStack dst{kDirt, 60}, src{kDirt, 10};
  CHECK(mergeInto(dst, src) == 4);
  CHECK(dst == ItemStack{kDirt, 64});
  CHECK(src == ItemStack{kDirt, 6});
}

TEST_CASE("merging into a full stack moves nothing") {
  ItemStack dst{kDirt, 64}, src{kDirt, 10};
  CHECK(mergeInto(dst, src) == 0);
  CHECK(dst == ItemStack{kDirt, 64});
  CHECK(src == ItemStack{kDirt, 10});
}

TEST_CASE("different items don't merge") {
  ItemStack dst{kDirt, 5}, src{kSand, 5};
  CHECK(mergeInto(dst, src) == 0);
  CHECK(dst == ItemStack{kDirt, 5});
  CHECK(src == ItemStack{kSand, 5});
}

TEST_CASE("merging an empty source does nothing") {
  ItemStack dst{kDirt, 5}, src;
  CHECK(mergeInto(dst, src) == 0);
  CHECK(dst == ItemStack{kDirt, 5});
}

TEST_CASE("an oversized source merges up to the max") {
  ItemStack dst, src{kDirt, 100};
  CHECK(mergeInto(dst, src) == 64);
  CHECK(dst == ItemStack{kDirt, 64});
  CHECK(src == ItemStack{kDirt, 36});
}

TEST_CASE("split takes up to n") {
  ItemStack s{ItemId::DIAMOND, 10};
  CHECK(split(s, 3) == ItemStack{ItemId::DIAMOND, 3});
  CHECK(s == ItemStack{ItemId::DIAMOND, 7});

  CHECK(split(s, 0) == ItemStack{});
  CHECK(s == ItemStack{ItemId::DIAMOND, 7});

  CHECK(split(s, 50) == ItemStack{ItemId::DIAMOND, 7});
  CHECK(s == ItemStack{});

  ItemStack empty;
  CHECK(split(empty, 5) == ItemStack{});
}

TEST_CASE("berries, sweet petals, fish and crops are the only food") {
  CHECK(itemDef(ItemId::BERRIES).food_heal == 4);
  CHECK(itemDef(ItemId::SWEET_PETALS).food_heal == 6);
  for (uint16_t v = 1; v <= static_cast<uint16_t>(kLastMaterialItem); ++v) {
    ItemId id = static_cast<ItemId>(v);
    if (!isValidItem(id) || id == ItemId::BERRIES || id == ItemId::SWEET_PETALS) continue;
    if (id >= ItemId::RAW_PEBBLEFISH && id <= ItemId::COOKED_GLOWFIN) continue;
    if (id == ItemId::BREAD || id == ItemId::CARROT || id == ItemId::STRAWBERRY) continue;
    CAPTURE(itemDef(id).name);
    CHECK(itemDef(id).food_heal == 0);
  }
}

TEST_CASE("stacks keep their wear, and only merge at equal wear") {
  ItemStack worn{ItemId::COAL, 3, 7};
  ItemStack dst{};
  CHECK(mergeInto(dst, worn) == 3);
  CHECK(dst == ItemStack{ItemId::COAL, 3, 7});
  ItemStack fresh{ItemId::COAL, 2, 0};
  CHECK(mergeInto(dst, fresh) == 0);
  ItemStack part = split(dst, 1);
  CHECK(part.wear == 7);
  ItemStack a{ItemId::COAL, 1, 1}, b{ItemId::COAL, 1, 2};
  CHECK(moveStack(a, b, 0)); // swapped, not merged
  CHECK(a.wear == 2);
  CHECK(b.wear == 1);
}
