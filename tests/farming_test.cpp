#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "block/crop.hpp"
#include "item/farming.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include "item/recipes.hpp"
#include "item/tools.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <set>
#include <tuple>

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

// ─── Rules ─────────────────────────────────────────────────────────────

TEST_CASE("each crop has its seed, and only seeds plant") {
  for (int k = 0; k < kCropKinds; ++k) {
    CropKind kind = static_cast<CropKind>(k);
    CHECK(cropForSeed(seedFor(kind)) == kind);
  }
  CHECK(seedFor(CropKind::Wheat) == ItemId::WHEAT_SEEDS);
  CHECK(seedFor(CropKind::Strawberry) == ItemId::STRAWBERRY_SEEDS);
  CHECK_FALSE(cropForSeed(ItemId::WHEAT).has_value());
  CHECK_FALSE(cropForSeed(ItemId::BERRIES).has_value());
  CHECK_FALSE(cropForSeed(ItemId::NONE).has_value());
}

TEST_CASE("a hoe tills grass and dirt only") {
  CHECK(tillable(BlockType::GRASS));
  CHECK(tillable(BlockType::DIRT));
  for (BlockType b : {BlockType::SAND, BlockType::STONE, BlockType::SNOW_DIRT,
                      BlockType::FARMLAND_DRY, BlockType::FARMLAND_WET, BlockType::AIR})
    CHECK_FALSE(tillable(b));
}

TEST_CASE("tilling grass turns up a wheat or carrot seed about one time in six") {
  std::mt19937 rng(7);
  int wheat = 0, carrot = 0;
  const int tries = 6000;
  for (int i = 0; i < tries; ++i) {
    auto seed = seedFromTilling(BlockType::GRASS, rng);
    if (!seed) continue;
    REQUIRE((*seed == ItemId::WHEAT_SEEDS || *seed == ItemId::CARROT_SEEDS));
    (*seed == ItemId::WHEAT_SEEDS ? wheat : carrot)++;
  }
  CHECK(wheat + carrot == doctest::Approx(tries / kSeedOneIn).epsilon(0.15));
  CHECK(wheat == doctest::Approx(carrot).epsilon(0.25));
  for (int i = 0; i < 200; ++i) CHECK_FALSE(seedFromTilling(BlockType::DIRT, rng).has_value());
}

TEST_CASE("an unripe crop gives back its seed; a ripe one its harvest") {
  std::mt19937 rng(1);
  for (int k = 0; k < kCropKinds; ++k)
    for (int stage = 0; stage < kCropStages - 1; ++stage) {
      auto got = harvest(cropBlock(static_cast<CropKind>(k), stage), rng);
      REQUIRE(got.size() == 1);
      CHECK(got[0] == ItemStack{seedFor(static_cast<CropKind>(k)), 1});
    }
  for (int i = 0; i < 100; ++i) {
    auto wheat = harvest(BlockType::WHEAT_3, rng);
    REQUIRE(wheat.size() == 2);
    CHECK(wheat[0] == ItemStack{ItemId::WHEAT, 1});
    CHECK(wheat[1].id == ItemId::WHEAT_SEEDS);
    CHECK(wheat[1].count >= 1);
    CHECK(wheat[1].count <= 2);
    for (auto [crop, food, seed] : {std::tuple{BlockType::CARROT_3, ItemId::CARROT, ItemId::CARROT_SEEDS},
                                    std::tuple{BlockType::STRAWBERRY_3, ItemId::STRAWBERRY,
                                               ItemId::STRAWBERRY_SEEDS}}) {
      auto got = harvest(crop, rng);
      REQUIRE(got.size() == 2);
      CHECK(got[0].id == food);
      CHECK(got[0].count >= 2);
      CHECK(got[0].count <= 3);
      CHECK(got[1] == ItemStack{seed, 1});
    }
  }
  CHECK(harvest(BlockType::DIRT, rng).empty());
}

TEST_CASE("sprout to ripe takes about a day, give or take") {
  std::mt19937 rng(3);
  for (int i = 0; i < 500; ++i) {
    int ms = stageMs(rng);
    CHECK(ms >= kStageMs - kStageJitterMs);
    CHECK(ms <= kStageMs + kStageJitterMs);
  }
  // Three steps up from a sprout; a day is 20 minutes of play.
  CHECK((kCropStages - 1) * kStageMs == 20 * 60 * 1000);
  CHECK(kWetMs == 20 * 60 * 1000);
}

TEST_CASE("soil is near water within four blocks sideways, level or one below") {
  glm::ivec3 soil{10, 70, 10};
  auto withWaterAt = [](glm::ivec3 water) {
    return [water](glm::ivec3 p) { return p == water ? BlockType::WATER : BlockType::DIRT; };
  };
  CHECK(nearWater(soil, withWaterAt(soil + glm::ivec3(4, 0, 0))));
  CHECK(nearWater(soil, withWaterAt(soil + glm::ivec3(-4, -1, 4))));
  CHECK(nearWater(soil, withWaterAt(soil + glm::ivec3(1, 0, 0))));
  CHECK_FALSE(nearWater(soil, withWaterAt(soil + glm::ivec3(5, 0, 0))));
  CHECK_FALSE(nearWater(soil, withWaterAt(soil + glm::ivec3(0, 1, 0)))); // above doesn't count
  CHECK_FALSE(nearWater(soil, withWaterAt(soil + glm::ivec3(0, -2, 0))));
}

// ─── Aiming ────────────────────────────────────────────────────────────

TEST_CASE("a watering can's aim stops at water; other aims go through it") {
  ScopedRenderDistance rd{0};
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {2, 100, 2}, {6, 100, 6}, BlockType::STONE);
  fill(world, {2, 101, 2}, {6, 101, 6}, BlockType::WATER);
  fill(world, {2, 102, 2}, {6, 110, 6}, BlockType::AIR);
  const glm::vec3 eye{4.5f, 103.5f, 4.5f}, down{0.0f, -1.0f, 0.0f};

  RaycastResult dry = world.raycast(eye, down, 5.0f);
  REQUIRE(dry.hit);
  CHECK(dry.block_pos == glm::ivec3(4, 100, 4));
  CHECK(dry.block_type == BlockType::STONE);

  RaycastResult wet = world.raycast(eye, down, 5.0f, true);
  REQUIRE(wet.hit);
  CHECK(wet.block_pos == glm::ivec3(4, 101, 4));
  CHECK(wet.block_type == BlockType::WATER);
}
