#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "block/door.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include "item/recipes.hpp"

namespace {
const DoorFacing kFacings[] = {DoorFacing::North, DoorFacing::East, DoorFacing::South,
                               DoorFacing::West};
} // namespace

TEST_CASE("door block ids are pinned") {
  CHECK(static_cast<int>(BlockType::DOOR_BOTTOM_N_CLOSED) == 49);
  CHECK(static_cast<int>(BlockType::DOOR_TOP_W_OPEN) == 64);
}

TEST_CASE("every door state round-trips through its block type") {
  int n = 0;
  for (bool top : {false, true})
    for (DoorFacing f : kFacings)
      for (bool open : {false, true}) {
        BlockType b = doorBlock(top, f, open);
        CAPTURE(static_cast<int>(b));
        CHECK(isDoor(b));
        CHECK(doorTop(b) == top);
        CHECK(doorFacing(b) == f);
        CHECK(doorOpen(b) == open);
        CHECK(toggled(b) == doorBlock(top, f, !open));
        CHECK(toggled(toggled(b)) == b);
        ++n;
      }
  CHECK(n == 16);
  CHECK_FALSE(isDoor(BlockType::TREASURE_CHEST));
  CHECK_FALSE(isDoor(BlockType::AIR));
}

TEST_CASE("a door faces the way its placer looks") {
  CHECK(doorFacingForYaw(-90.0f) == DoorFacing::North); // the camera's default, -Z
  CHECK(doorFacingForYaw(0.0f) == DoorFacing::East);
  CHECK(doorFacingForYaw(90.0f) == DoorFacing::South);
  CHECK(doorFacingForYaw(180.0f) == DoorFacing::West);
  CHECK(doorFacingForYaw(-120.0f) == DoorFacing::North);
  CHECK(doorFacingForYaw(-160.0f) == DoorFacing::West);
}

TEST_CASE("the panel is thin, full height, and swings from the near side to the left") {
  for (DoorFacing f : kFacings) {
    CAPTURE(static_cast<int>(f));
    DoorBox closed = doorPanel(doorBlock(false, f, false));
    DoorBox open = doorPanel(doorBlock(false, f, true));
    for (const DoorBox &b : {closed, open}) {
      glm::vec3 size = b.hi - b.lo;
      CHECK(b.lo.y == 0.0f);
      CHECK(b.hi.y == 1.0f);
      CHECK(glm::min(size.x, size.z) == doctest::Approx(kDoorThickness));
      CHECK(glm::max(size.x, size.z) == 1.0f);
      CHECK(glm::all(glm::greaterThanEqual(b.lo, glm::vec3(0.0f))));
      CHECK(glm::all(glm::lessThanEqual(b.hi, glm::vec3(1.0f))));
    }
    // Closed across the way you looked, open along it.
    const bool along_z = f == DoorFacing::North || f == DoorFacing::South;
    CHECK(((closed.hi - closed.lo).x == 1.0f) == along_z);
    CHECK(((open.hi - open.lo).z == 1.0f) == along_z);
    // The halves of a door share its box.
    DoorBox top = doorPanel(doorBlock(true, f, false));
    CHECK(top.lo == closed.lo);
    CHECK(top.hi == closed.hi);
  }
  // Looking north (-Z): closed on the near (+Z) side, open along the left (-X).
  DoorBox n = doorPanel(BlockType::DOOR_BOTTOM_N_CLOSED);
  CHECK(n.lo.z == doctest::Approx(1.0f - kDoorThickness));
  DoorBox no = doorPanel(BlockType::DOOR_BOTTOM_N_OPEN);
  CHECK(no.lo.x == 0.0f);
  CHECK(no.hi.x == doctest::Approx(kDoorThickness));
}

TEST_CASE("door halves aren't items; the bottom drops one door, the top nothing") {
  for (int v = static_cast<int>(BlockType::DOOR_BOTTOM_N_CLOSED);
       v <= static_cast<int>(BlockType::DOOR_TOP_W_OPEN); ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(v);
    CHECK_FALSE(isValidItem(itemForBlock(b)));
    CHECK(dropFor(b) == (doorTop(b) ? std::nullopt : std::optional(ItemId::WOODEN_DOOR)));
    CHECK(blockName(b) == "Wooden Door");
    CHECK(block_data.count(b) == 1);
    CHECK(skyLightOpacity(b) == 0);
    // Mines like planks, and always drops.
    CHECK(breakMs(b, ItemId::NONE) == breakMs(BlockType::OAK_PLANKS, ItemId::NONE));
    CHECK(breakMs(b, ItemId::STONE_AXE) == breakMs(BlockType::OAK_PLANKS, ItemId::STONE_AXE));
    CHECK(dropsWith(b, ItemId::NONE));
    // The window in the top half is a see-through cutout.
    CHECK(isCutout(b) == doorTop(b));
    // Whatever is next to a door still shows its face.
    CHECK(shouldRenderFace(BlockType::STONE, b));
  }
}

TEST_CASE("the wooden door: stacks to 16, in the palette, 6 planks make 2 at a table") {
  REQUIRE(isValidItem(ItemId::WOODEN_DOOR));
  CHECK(itemDef(ItemId::WOODEN_DOOR).name == "Wooden Door");
  CHECK(itemDef(ItemId::WOODEN_DOOR).max_stack == 16);
  CHECK_FALSE(itemDef(ItemId::WOODEN_DOOR).places.has_value()); // the Game places doors
  const auto &palette = paletteItems();
  CHECK(std::find(palette.begin(), palette.end(), ItemId::WOODEN_DOOR) != palette.end());
  int doors = 0;
  for (const Recipe &r : recipes()) {
    if (r.makes.id != ItemId::WOODEN_DOOR) continue;
    ++doors;
    CHECK(r.needs_table);
    CHECK(r.makes.count == 2);
    REQUIRE(r.needs.size() == 1);
    CHECK(r.needs[0].count == 6);
    CHECK(r.needs[0].any_of.size() == 4); // any planks
  }
  CHECK(doors == 1);
}
