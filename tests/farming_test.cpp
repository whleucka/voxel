#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "block/crop.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
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
