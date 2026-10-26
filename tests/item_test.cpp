#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "item/item.hpp"
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
    if (b == BlockType::WATER) continue;
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
  CHECK(mats.size() == 7);

  std::set<std::pair<int, int>> tiles;
  for (ItemId id : mats) {
    REQUIRE(isValidItem(id));
    const ItemDef &def = itemDef(id);
    CAPTURE(def.name);
    CHECK(!def.name.empty());
    CHECK(def.name != "Unknown");
    CHECK(def.max_stack == 64);
    CHECK_FALSE(def.places.has_value());
    CHECK(def.icon.atlas == ItemIcon::Atlas::Item);
    CHECK(def.icon.tile.x >= 0);
    CHECK(def.icon.tile.x < 8);
    CHECK(def.icon.tile.y >= 0);
    CHECK(def.icon.tile.y < 8);
    tiles.insert({def.icon.tile.x, def.icon.tile.y});
  }
  CHECK(tiles.size() == mats.size());
}

TEST_CASE("material item ids are stable") {
  // Persisted in saves: these numbers must never change.
  CHECK(static_cast<uint16_t>(ItemId::COAL) == 256);
  CHECK(static_cast<uint16_t>(ItemId::RAW_COPPER) == 262);
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
      BlockType::COPPER_ORE,
  };
  for (uint16_t v = 0; v <= kLastBlock; ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(blockName(b));
    auto drop = dropFor(b);
    if (drop) CHECK(isValidItem(*drop));
    if (!special.count(b)) CHECK(drop == itemForBlock(b));
  }
}
