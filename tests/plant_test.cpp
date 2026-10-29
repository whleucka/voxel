#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include <algorithm>

namespace {

bool inPalette(BlockType b) {
  const auto &palette = paletteItems();
  return std::find(palette.begin(), palette.end(), itemForBlock(b)) != palette.end();
}

const BlockType kWildPlants[] = {
    BlockType::TALL_GRASS, BlockType::POPPY,       BlockType::BUTTERCUP,
    BlockType::CORNFLOWER, BlockType::PINK_TULIP,  BlockType::DAISY,
    BlockType::LAVENDER,   BlockType::HIBISCUS,    BlockType::FERN,
    BlockType::RED_MUSHROOM, BlockType::BROWN_MUSHROOM, BlockType::DEAD_BUSH,
    BlockType::BERRY_BUSH, BlockType::BERRY_BUSH_BARE, BlockType::SHELLS};

} // namespace

// ─── Blocks ────────────────────────────────────────────────────────────

TEST_CASE("plant block ids are pinned: saves store them as numbers") {
  CHECK(static_cast<int>(BlockType::TALL_GRASS) == 143);
  CHECK(static_cast<int>(BlockType::SHELLS) == 157);
  CHECK(static_cast<int>(BlockType::CACTUS) == 158);
  CHECK(static_cast<int>(BlockType::TORCH) == 162);
  CHECK(static_cast<int>(BlockType::CAKE_4) == 167);
  CHECK(static_cast<int>(BlockType::SNOW_BRICKS) == 172);
  CHECK(kLastBlockType == BlockType::SNOW_BRICKS);
}

TEST_CASE("wild plants are named cutouts you walk through and pick at once") {
  for (BlockType b : kWildPlants) {
    CAPTURE(blockName(b));
    CHECK(isPlant(b));
    CHECK(blockName(b) != "Unknown");
    CHECK_FALSE(blocksMovement(b));
    CHECK(isPassable(b));
    CHECK(isCutout(b));
    CHECK(skyLightOpacity(b) == 0);
    CHECK(hasOwnShape(b));
    CHECK(hasFlatIcon(b));
    CHECK(needsSupport(b));
    CHECK(breakMs(b, ItemId::NONE) == 0);
  }
  CHECK(isFlower(BlockType::POPPY));
  CHECK(isFlower(BlockType::HIBISCUS));
  CHECK_FALSE(isFlower(BlockType::TALL_GRASS));
  CHECK_FALSE(isFlower(BlockType::FERN));
}

TEST_CASE("plants drop themselves, except grass, dead bushes, picked bushes and shells") {
  CHECK(dropFor(BlockType::POPPY) == itemForBlock(BlockType::POPPY));
  CHECK(dropFor(BlockType::RED_MUSHROOM) == itemForBlock(BlockType::RED_MUSHROOM));
  CHECK(dropFor(BlockType::BERRY_BUSH) == itemForBlock(BlockType::BERRY_BUSH));
  CHECK(dropFor(BlockType::BERRY_BUSH_BARE) == itemForBlock(BlockType::BERRY_BUSH));
  CHECK(dropFor(BlockType::TALL_GRASS) == std::nullopt); // the Game rolls for a seed
  CHECK(dropFor(BlockType::DEAD_BUSH) == ItemId::STICKS);
  CHECK(dropFor(BlockType::SHELLS) == ItemId::SEA_SHELL);
}

TEST_CASE("plants are in the creative palette, but not a picked bush or shells") {
  CHECK(inPalette(BlockType::POPPY));
  CHECK(inPalette(BlockType::TALL_GRASS));
  CHECK(inPalette(BlockType::BERRY_BUSH));
  CHECK(inPalette(BlockType::CACTUS));
  CHECK(inPalette(BlockType::BLUE_CORAL));
  CHECK(inPalette(BlockType::SANDSTONE_BRICKS));
  CHECK(inPalette(BlockType::SNOW_BRICKS));
  CHECK_FALSE(inPalette(BlockType::BERRY_BUSH_BARE));
  CHECK_FALSE(inPalette(BlockType::SHELLS));
}

TEST_CASE("each plant grows on its own ground") {
  CHECK(growsOn(BlockType::POPPY, BlockType::GRASS, true));
  CHECK(growsOn(BlockType::TALL_GRASS, BlockType::DIRT, true));
  CHECK_FALSE(growsOn(BlockType::POPPY, BlockType::STONE, true));
  CHECK_FALSE(growsOn(BlockType::POPPY, BlockType::SAND, true));
  CHECK(growsOn(BlockType::BERRY_BUSH, BlockType::GRASS, true));
  CHECK(growsOn(BlockType::DEAD_BUSH, BlockType::SAND, true));
  CHECK_FALSE(growsOn(BlockType::DEAD_BUSH, BlockType::GRASS, true));
  CHECK(growsOn(BlockType::SHELLS, BlockType::SAND, true));
  CHECK(growsOn(BlockType::CACTUS, BlockType::SAND, true));
  CHECK(growsOn(BlockType::CACTUS, BlockType::CACTUS, true));
  CHECK_FALSE(growsOn(BlockType::CACTUS, BlockType::GRASS, true));
  // Mushrooms grow on any solid block, in the dark too.
  CHECK(growsOn(BlockType::BROWN_MUSHROOM, BlockType::STONE, true));
  CHECK_FALSE(growsOn(BlockType::BROWN_MUSHROOM, BlockType::OAK_LEAF, false));
}

TEST_CASE("cactus is solid, and corals and bricks are plain blocks") {
  CHECK(blocksMovement(BlockType::CACTUS));
  CHECK(needsSupport(BlockType::CACTUS));
  CHECK(hasOwnShape(BlockType::CACTUS));
  CHECK_FALSE(isSolidGround(BlockType::CACTUS)); // nothing hangs on it
  CHECK(shouldRenderFace(BlockType::STONE, BlockType::CACTUS));
  for (BlockType b : {BlockType::PINK_CORAL, BlockType::BLUE_CORAL, BlockType::YELLOW_CORAL,
                      BlockType::SANDSTONE_BRICKS, BlockType::SNOW_BRICKS}) {
    CAPTURE(blockName(b));
    CHECK(blocksMovement(b));
    CHECK(isSolidGround(b));
    CHECK_FALSE(isCutout(b));
    CHECK_FALSE(hasOwnShape(b));
    CHECK(dropFor(b) == itemForBlock(b));
  }
  CHECK(isCoral(BlockType::BLUE_CORAL));
}

// ─── Torches ───────────────────────────────────────────────────────────

TEST_CASE("torches glow, are walked through, and all drop a torch") {
  for (int v = static_cast<int>(BlockType::TORCH); v <= static_cast<int>(BlockType::TORCH_W);
       ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(v);
    CHECK(isTorch(b));
    CHECK(blockName(b) == "Torch");
    CHECK(blockLightEmission(b) == 13);
    CHECK_FALSE(blocksMovement(b));
    CHECK(dropFor(b) == itemForBlock(BlockType::TORCH));
    CHECK(breakMs(b, ItemId::NONE) == 0);
  }
  CHECK(isValidItem(itemForBlock(BlockType::TORCH)));
  CHECK_FALSE(isValidItem(itemForBlock(BlockType::TORCH_N)));
  CHECK(inPalette(BlockType::TORCH));
}

TEST_CASE("a torch goes on a floor or a wall, never a ceiling") {
  CHECK(torchFor({0, -1, 0}) == BlockType::TORCH);
  CHECK(torchFor({0, 0, -1}) == BlockType::TORCH_N);
  CHECK(torchFor({1, 0, 0}) == BlockType::TORCH_E);
  CHECK(torchFor({0, 0, 1}) == BlockType::TORCH_S);
  CHECK(torchFor({-1, 0, 0}) == BlockType::TORCH_W);
  CHECK(torchFor({0, 1, 0}) == std::nullopt);
  for (glm::ivec3 d : {glm::ivec3(0, -1, 0), glm::ivec3(0, 0, -1), glm::ivec3(1, 0, 0),
                       glm::ivec3(0, 0, 1), glm::ivec3(-1, 0, 0)})
    CHECK(torchSupport(*torchFor(d)) == d);
}

// ─── Cake ──────────────────────────────────────────────────────────────

TEST_CASE("a cake counts its slices down and is placed whole") {
  CHECK(cakeSlices(BlockType::CAKE_4) == 4);
  CHECK(cakeSlices(BlockType::CAKE_1) == 1);
  for (int n = 1; n <= kCakeSlices; ++n) CHECK(cakeSlices(cakeBlock(n)) == n);
  CHECK(blockName(BlockType::CAKE_2) == "Cake");
  CHECK(blocksMovement(BlockType::CAKE_4));
  CHECK(blockHeight(BlockType::CAKE_4) == kCakeHeight);
  CHECK(dropFor(BlockType::CAKE_4) == std::nullopt); // eaten, not picked up
  CHECK(isValidItem(itemForBlock(BlockType::CAKE_4)));
  CHECK_FALSE(isValidItem(itemForBlock(BlockType::CAKE_3)));
  CHECK(itemDef(itemForBlock(BlockType::CAKE_4)).name == "Cake");
}
