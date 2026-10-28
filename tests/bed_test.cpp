#include "doctest/doctest.h"
#include "block/bed.hpp"
#include "block/block_data.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include "item/recipes.hpp"
#include <algorithm>

namespace {
const DoorFacing kFacings[] = {DoorFacing::North, DoorFacing::East, DoorFacing::South,
                               DoorFacing::West};
} // namespace

TEST_CASE("bed block ids are pinned") {
  CHECK(static_cast<int>(BlockType::BED_WHITE_FOOT_N) == 65);
  CHECK(static_cast<int>(BlockType::BED_RED_FOOT_N) == 73);
  CHECK(static_cast<int>(BlockType::BED_PINK_HEAD_W) == 128);
  CHECK(kLastBlockType == BlockType::BED_PINK_HEAD_W);
}

TEST_CASE("every bed state round-trips through its block type") {
  int n = 0;
  for (int c = 0; c < kBedColours; ++c)
    for (bool head : {false, true})
      for (DoorFacing f : kFacings) {
        BedColour colour = static_cast<BedColour>(c);
        BlockType b = bedBlock(colour, head, f);
        CAPTURE(static_cast<int>(b));
        CHECK(isBed(b));
        CHECK_FALSE(isDoor(b));
        CHECK(bedColour(b) == colour);
        CHECK(bedHead(b) == head);
        CHECK(bedFacing(b) == f);
        ++n;
      }
  CHECK(n == 64);
  CHECK_FALSE(isBed(BlockType::DOOR_TOP_W_OPEN));
  CHECK_FALSE(isBed(BlockType::AIR));
}

TEST_CASE("the head lies one cell the way the placer looked, and each half finds the other") {
  const glm::ivec3 foot{10, 64, -3};
  CHECK(facingStep(DoorFacing::North) == glm::ivec3(0, 0, -1));
  CHECK(facingStep(DoorFacing::East) == glm::ivec3(1, 0, 0));
  for (DoorFacing f : kFacings) {
    CAPTURE(static_cast<int>(f));
    BlockType foot_half = bedBlock(BedColour::Blue, false, f);
    BlockType head_half = bedBlock(BedColour::Blue, true, f);
    glm::ivec3 head = bedOther(foot, foot_half);
    CHECK(head == foot + facingStep(f));
    CHECK(bedOther(head, head_half) == foot);
    CHECK(bedFoot(head, head_half) == foot);
    CHECK(bedFoot(foot, foot_half) == foot);
  }
}

TEST_CASE("bed halves aren't items; the foot drops a bed of its colour, the head nothing") {
  for (int v = static_cast<int>(BlockType::BED_WHITE_FOOT_N);
       v <= static_cast<int>(BlockType::BED_PINK_HEAD_W); ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(v);
    CHECK_FALSE(isValidItem(itemForBlock(b)));
    CHECK(dropFor(b) == (bedHead(b) ? std::nullopt : std::optional(bedItem(bedColour(b)))));
    CHECK(blockName(b) == itemDef(bedItem(bedColour(b))).name);
    CHECK(block_data.count(b) == 1);
    CHECK(skyLightOpacity(b) == 0);
    CHECK(blocksMovement(b));
    CHECK_FALSE(isCutout(b));
    // Mines like planks, and always drops.
    CHECK(breakMs(b, ItemId::NONE) == breakMs(BlockType::OAK_PLANKS, ItemId::NONE));
    CHECK(breakMs(b, ItemId::STONE_AXE) == breakMs(BlockType::OAK_PLANKS, ItemId::STONE_AXE));
    CHECK(dropsWith(b, ItemId::NONE));
    // A bed is low: whatever is next to it still shows its face.
    CHECK(shouldRenderFace(BlockType::STONE, b));
  }
  CHECK(blockName(BlockType::BED_PINK_FOOT_E) == "Pink Bed");
}

TEST_CASE("each colour has its own blanket tile") {
  for (int c = 0; c < kBedColours; ++c)
    for (int d = c + 1; d < kBedColours; ++d)
      CHECK(bedBlanketTile(static_cast<BedColour>(c)) != bedBlanketTile(static_cast<BedColour>(d)));
  CHECK(bedBlanketTile(BedColour::White) == glm::ivec2(15, 3));
  CHECK(bedBlanketTile(BedColour::Pink) == glm::ivec2(6, 4));
}

TEST_CASE("the 8 beds: stack to 16, in the palette, and not block items") {
  const auto &palette = paletteItems();
  for (int c = 0; c < kBedColours; ++c) {
    ItemId id = bedItem(static_cast<BedColour>(c));
    CAPTURE(c);
    REQUIRE(isValidItem(id));
    CHECK(isBedItem(id));
    CHECK(bedItemColour(id) == static_cast<BedColour>(c));
    CHECK(itemDef(id).max_stack == 16);
    CHECK_FALSE(itemDef(id).places.has_value()); // the Game places beds
    CHECK(std::find(palette.begin(), palette.end(), id) != palette.end());
  }
  CHECK(itemDef(ItemId::WHITE_BED).name == "White Bed");
  CHECK(itemDef(ItemId::PURPLE_BED).name == "Purple Bed");
  CHECK_FALSE(isBedItem(ItemId::WOODEN_DOOR));
}

namespace {

const Recipe *recipeFor(ItemId id) {
  const Recipe *found = nullptr;
  for (const Recipe &r : recipes())
    if (r.makes.id == id) {
      CHECK(found == nullptr); // just one
      found = &r;
    }
  return found;
}

} // namespace

TEST_CASE("3 wool and 3 planks of any wood make a white bed at a table") {
  const Recipe *r = recipeFor(ItemId::WHITE_BED);
  REQUIRE(r);
  CHECK(r->needs_table);
  CHECK(r->makes.count == 1);
  REQUIRE(r->needs.size() == 2);
  CHECK(r->needs[0].any_of == std::vector<ItemId>{ItemId::WOOL});
  CHECK(r->needs[0].count == 3);
  CHECK(r->needs[1].any_of.size() == 4); // any planks
  CHECK(r->needs[1].count == 3);
}

TEST_CASE("a white bed takes the coloured blocks' dyes") {
  const std::pair<ItemId, ItemId> dyed[] = {
      {ItemId::RED_BED, ItemId::RUBY},         {ItemId::ORANGE_BED, ItemId::RAW_COPPER},
      {ItemId::YELLOW_BED, ItemId::RAW_GOLD},  {ItemId::GREEN_BED, ItemId::EMERALD},
      {ItemId::BLUE_BED, ItemId::SEA_SHELL},   {ItemId::PURPLE_BED, ItemId::BERRIES},
      {ItemId::PINK_BED, ItemId::SWEET_PETALS}};
  for (const auto &[bed, dye] : dyed) {
    CAPTURE(itemDef(bed).name);
    const Recipe *r = recipeFor(bed);
    REQUIRE(r);
    CHECK(r->needs_table);
    CHECK(r->makes.count == 1);
    REQUIRE(r->needs.size() == 2);
    CHECK(r->needs[0].any_of == std::vector<ItemId>{ItemId::WHITE_BED});
    CHECK(r->needs[0].count == 1);
    CHECK(r->needs[1].any_of == std::vector<ItemId>{dye});
    CHECK(r->needs[1].count == 1);
  }
}
