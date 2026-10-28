#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "block/crop.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include "item/recipes.hpp"
#include "item/tools.hpp"
#include <set>

// ─── Blocks ────────────────────────────────────────────────────────────

TEST_CASE("farming block ids are pinned") {
  CHECK(static_cast<int>(BlockType::FARMLAND_DRY) == 129);
  CHECK(static_cast<int>(BlockType::STRAWBERRY_3) == 142);
  CHECK(kLastBlockType == BlockType::STRAWBERRY_3);
}

TEST_CASE("every crop round-trips through its block type") {
  int n = 0;
  for (int k = 0; k < kCropKinds; ++k)
    for (int stage = 0; stage < kCropStages; ++stage) {
      BlockType b = cropBlock(static_cast<CropKind>(k), stage);
      CAPTURE(static_cast<int>(b));
      CHECK(isCrop(b));
      CHECK(cropKind(b) == static_cast<CropKind>(k));
      CHECK(cropStage(b) == stage);
      CHECK(cropRipe(b) == (stage == kCropStages - 1));
      ++n;
    }
  CHECK(n == 12);
  CHECK_FALSE(isCrop(BlockType::FARMLAND_WET));
  CHECK_FALSE(isCrop(BlockType::BED_PINK_HEAD_W));
  CHECK(isFarmland(BlockType::FARMLAND_DRY));
  CHECK(isFarmland(BlockType::FARMLAND_WET));
  CHECK_FALSE(isFarmland(BlockType::DIRT));
}

TEST_CASE("crops are see-through plants you walk through, picked at once") {
  for (int v = static_cast<int>(BlockType::WHEAT_0); v <= static_cast<int>(BlockType::STRAWBERRY_3);
       ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(blockName(b));
    CHECK_FALSE(blocksMovement(b));
    CHECK(isCutout(b));
    CHECK(skyLightOpacity(b) == 0);
    CHECK(blockLightEmission(b) == 0);
    CHECK(breakMs(b, ItemId::NONE) == 0);
    CHECK_FALSE(dropFor(b).has_value()); // the Game hands out the harvest
    CHECK_FALSE(isValidItem(itemForBlock(b)));
  }
  CHECK(blockName(BlockType::WHEAT_0) == "Wheat");
  CHECK(blockName(BlockType::CARROT_3) == "Carrots");
  CHECK(blockName(BlockType::STRAWBERRY_2) == "Strawberries");
}

TEST_CASE("every crop stage has its own tile") {
  std::set<std::pair<int, int>> tiles;
  for (int v = static_cast<int>(BlockType::WHEAT_0); v <= static_cast<int>(BlockType::STRAWBERRY_3);
       ++v) {
    glm::ivec2 t = cropTile(static_cast<BlockType>(v));
    CHECK(t.x >= 0);
    CHECK(t.x < 16);
    tiles.insert({t.x, t.y});
  }
  CHECK(tiles.size() == 12);
  CHECK_FALSE(tiles.count({kFarmlandWetSide.x, kFarmlandWetSide.y}));
  CHECK_FALSE(tiles.count({kCropTopTile.x, kCropTopTile.y}));
}

TEST_CASE("farmland is solid soil that digs and drops like dirt, and isn't an item") {
  for (BlockType b : {BlockType::FARMLAND_DRY, BlockType::FARMLAND_WET}) {
    CAPTURE(blockName(b));
    CHECK(blocksMovement(b));
    CHECK_FALSE(isCutout(b));
    CHECK(skyLightOpacity(b) == 15);
    CHECK(breakMs(b, ItemId::NONE) == breakMs(BlockType::DIRT, ItemId::NONE));
    CHECK(dropFor(b) == itemForBlock(BlockType::DIRT));
    CHECK_FALSE(isValidItem(itemForBlock(b)));
  }
  CHECK(block_data.at(BlockType::FARMLAND_DRY).top != block_data.at(BlockType::FARMLAND_WET).top);
}

// ─── Items ─────────────────────────────────────────────────────────────

TEST_CASE("the farming items: hoes and the can one to a stack, the rest 64") {
  for (ItemId id : {ItemId::WHEAT_SEEDS, ItemId::CARROT_SEEDS, ItemId::STRAWBERRY_SEEDS,
                    ItemId::WHEAT, ItemId::BREAD, ItemId::CARROT, ItemId::STRAWBERRY}) {
    CAPTURE(itemDef(id).name);
    CHECK(itemDef(id).max_stack == 64);
    CHECK(usesOf(id) == 0);
  }
  CHECK(itemDef(ItemId::WATERING_CAN).max_stack == 1);
  CHECK(itemDef(ItemId::WATERING_CAN).name == "Watering Can");
  CHECK(usesOf(ItemId::WATERING_CAN) == kCanSplashes);
  CHECK(toolOf(ItemId::WATERING_CAN).kind == ToolKind::None); // breaking blocks doesn't spill it
  for (ItemId id = ItemId::WOOD_HOE; id <= ItemId::DIAMOND_HOE;
       id = static_cast<ItemId>(static_cast<uint16_t>(id) + 1)) {
    CAPTURE(itemDef(id).name);
    CHECK(itemDef(id).max_stack == 1);
    CHECK(toolOf(id).kind == ToolKind::Hoe);
    CHECK(usesOf(id) == maxUses(toolOf(id).tier));
  }
}

TEST_CASE("bread, carrots and strawberries are food; wheat and seeds aren't") {
  CHECK(itemDef(ItemId::BREAD).food_heal == 8);
  CHECK(itemDef(ItemId::CARROT).food_heal == 4);
  CHECK(itemDef(ItemId::STRAWBERRY).food_heal == 4);
  CHECK(itemDef(ItemId::WHEAT).food_heal == 0);
  CHECK(itemDef(ItemId::WHEAT_SEEDS).food_heal == 0);
  CHECK(itemDef(ItemId::STRAWBERRY_SEEDS).food_heal == 0);
}

namespace {
const Recipe *recipeFor(ItemId id) {
  for (const Recipe &r : recipes())
    if (r.makes.id == id) return &r;
  return nullptr;
}
} // namespace

TEST_CASE("bread, strawberry seeds and the watering can are made at a table") {
  const Recipe *bread = recipeFor(ItemId::BREAD);
  REQUIRE(bread);
  CHECK(bread->needs_table);
  CHECK(bread->makes.count == 1);
  REQUIRE(bread->needs.size() == 1);
  CHECK(bread->needs[0].any_of == std::vector<ItemId>{ItemId::WHEAT});
  CHECK(bread->needs[0].count == 3);

  const Recipe *seeds = recipeFor(ItemId::STRAWBERRY_SEEDS);
  REQUIRE(seeds);
  CHECK(seeds->needs_table);
  CHECK(seeds->makes.count == 2);
  REQUIRE(seeds->needs.size() == 1);
  CHECK(seeds->needs[0].any_of == std::vector<ItemId>{ItemId::BERRIES});
  CHECK(seeds->needs[0].count == 1);

  const Recipe *can = recipeFor(ItemId::WATERING_CAN);
  REQUIRE(can);
  CHECK(can->needs_table);
  REQUIRE(can->needs.size() == 1);
  CHECK(can->needs[0].any_of == std::vector<ItemId>{ItemId::COPPER_INGOT});
  CHECK(can->needs[0].count == 3);

  // Wheat and carrot seeds come from tilling grass, not a recipe.
  CHECK_FALSE(recipeFor(ItemId::WHEAT_SEEDS));
  CHECK_FALSE(recipeFor(ItemId::CARROT_SEEDS));
}
