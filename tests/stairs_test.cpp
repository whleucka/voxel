#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include "ui/minimap.hpp"
#include <algorithm>

namespace {

bool inPalette(BlockType b) {
  const auto &palette = paletteItems();
  return std::find(palette.begin(), palette.end(), itemForBlock(b)) != palette.end();
}

constexpr DoorFacing kFacings[] = {DoorFacing::North, DoorFacing::East, DoorFacing::South,
                                   DoorFacing::West};

} // namespace

// ─── Blocks ────────────────────────────────────────────────────────────

TEST_CASE("slab and stair block ids are pinned: saves store them as numbers") {
  CHECK(static_cast<int>(BlockType::OAK_SLAB) == 173);
  CHECK(static_cast<int>(BlockType::SNOW_BRICK_SLAB) == 181);
  CHECK(static_cast<int>(BlockType::OAK_STAIRS_N) == 182);
  CHECK(static_cast<int>(BlockType::SNOW_BRICK_STAIRS_W) == 217);
  CHECK(kLastBlockType == BlockType::SNOW_BRICK_STAIRS_W);
}

TEST_CASE("slabs and stairs are named, mined and coloured like their material") {
  for (int m = 0; m < kShapeMaterials; ++m) {
    const BlockType material = kShapeMaterial[m];
    const BlockType slab = slabBlock(m);
    CAPTURE(blockName(material));
    CHECK(isSlab(slab));
    CHECK(shapeMaterial(slab) == material);
    CHECK(blockName(slab) == kSlabNames[m]);
    CHECK(itemDef(itemForBlock(slab)).max_stack == 64);
    CHECK(dropFor(slab) == itemForBlock(slab));
    for (BlockType b : {slab, stairBlock(m, DoorFacing::North), stairBlock(m, DoorFacing::West)}) {
      CHECK(breakMs(b, ItemId::NONE) == breakMs(material, ItemId::NONE));
      CHECK(toughness(b).best == toughness(material).best);
      CHECK(toughness(b).needs == toughness(material).needs);
      CHECK(block_data.at(b).side == block_data.at(material).side);
      CHECK(block_data.at(b).top == block_data.at(material).top);
      CHECK(mapColour(b) == mapColour(material));
      CHECK(blocksMovement(b));
      CHECK(hasOwnShape(b));
      CHECK_FALSE(hasFlatIcon(b));
      CHECK_FALSE(isSolidGround(b)); // a torch would float on the open half
      CHECK(skyLightOpacity(b) == 0);
      CHECK(shouldRenderFace(material, b));
    }
    for (DoorFacing f : kFacings) {
      const BlockType stair = stairBlock(m, f);
      CHECK(isStair(stair));
      CHECK(stairFacing(stair) == f);
      CHECK(shapeMaterial(stair) == material);
      CHECK(blockName(stair) == kStairNames[m]);
      // Whichever way it faces, it's the one stair item.
      CHECK(dropFor(stair) == itemForBlock(stairBlock(m, DoorFacing::North)));
    }
  }
  CHECK(blockName(BlockType::OAK_STAIRS_E) == "Oak Stairs");
  CHECK(blockName(BlockType::COBBLESTONE_SLAB) == "Cobblestone Slab");
}

TEST_CASE("every slab and the north stair are items in the palette; the turned stairs aren't") {
  for (int m = 0; m < kShapeMaterials; ++m) {
    CHECK(inPalette(slabBlock(m)));
    CHECK(inPalette(stairBlock(m, DoorFacing::North)));
    for (DoorFacing f : {DoorFacing::East, DoorFacing::South, DoorFacing::West}) {
      CHECK_FALSE(isValidItem(itemForBlock(stairBlock(m, f))));
      CHECK_FALSE(inPalette(stairBlock(m, f)));
    }
  }
}

// ─── Shapes ────────────────────────────────────────────────────────────

TEST_CASE("a slab fills the bottom half of its cell") {
  CellBoxes boxes = blockBoxes(BlockType::STONE_BRICK_SLAB);
  REQUIRE(boxes.count == 1);
  CHECK(boxes.box[0].lo == glm::vec3(0, 0, 0));
  CHECK(boxes.box[0].hi == glm::vec3(1, 0.5f, 1));
  CHECK(blockHeight(BlockType::STONE_BRICK_SLAB) == 0.5f);
}

TEST_CASE("a stair is a slab with its upper step at the back, the way it faces") {
  for (DoorFacing f : kFacings) {
    CAPTURE(static_cast<int>(f));
    CellBoxes boxes = blockBoxes(stairBlock(0, f));
    REQUIRE(boxes.count == 2);
    CHECK(boxes.box[0].hi == glm::vec3(1, 0.5f, 1));
    const DoorBox step = boxes.box[1];
    CHECK(step.lo.y == 0.5f);
    CHECK(step.hi.y == 1.0f);
    // Half the cell, on the side `facingStep` points to.
    const glm::vec3 size = step.hi - step.lo;
    CHECK(size.x * size.z == doctest::Approx(0.5f));
    const glm::vec3 centre = (step.lo + step.hi) * 0.5f - glm::vec3(0.5f);
    const glm::vec3 toward(facingStep(f));
    CHECK(centre.x == doctest::Approx(toward.x * 0.25f));
    CHECK(centre.z == doctest::Approx(toward.z * 0.25f));
  }
}

TEST_CASE("full blocks are one box, low blocks one low box, and air none") {
  CHECK(blockBoxes(BlockType::STONE).count == 1);
  CHECK(blockBoxes(BlockType::STONE).box[0].hi == glm::vec3(1));
  CHECK(blockBoxes(BlockType::CAKE_4).box[0].hi.y == kCakeHeight);
  CHECK(blockBoxes(BlockType::BED_RED_FOOT_N).box[0].hi.y == kBedHeight);
  CHECK(blockBoxes(BlockType::AIR).count == 0);
  CHECK(blockBoxes(BlockType::POPPY).count == 0);
  CHECK(insideBlock(BlockType::OAK_STAIRS_N, {0.5f, 0.75f, 0.25f}));      // the step
  CHECK_FALSE(insideBlock(BlockType::OAK_STAIRS_N, {0.5f, 0.75f, 0.75f})); // the open half
  CHECK(insideBlock(BlockType::OAK_STAIRS_N, {0.5f, 0.25f, 0.75f}));
}
