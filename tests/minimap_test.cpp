// The minimap's picture (surface colours and the cave slice), when it
// switches between them, and where markers go on the turning map.
#include "doctest/doctest.h"
#include "block/bed.hpp"
#include "test_util.hpp"
#include "ui/minimap.hpp"
#include "world/world.hpp"

namespace {

constexpr int kTop = 99; // the stone's top in the rig

// Chunk (0,0) only: stone up to kTop with clear sky above, and a tunnel
// sealed in the rock at y 50-51 along z = 8, from x = 2 to 13.
struct MapRig {
  World world;
  MapRig() {
    world.loadChunkSync(0, 0);
    fill(world, {0, 0, 0}, {kChunkWidth - 1, kChunkHeight - 1, kChunkDepth - 1}, BlockType::AIR);
    fill(world, {0, 0, 0}, {kChunkWidth - 1, kTop, kChunkDepth - 1}, BlockType::STONE);
    fill(world, {2, 50, 8}, {13, 51, 8}, BlockType::AIR);
  }
  glm::vec3 inTunnel() const { return {4.5f, 50.0f, 8.5f}; }
  glm::vec3 onTop() const { return {8.5f, kTop + 1.0f, 8.5f}; }
};

int brightness(glm::u8vec4 c) { return c.r + c.g + c.b; }

} // namespace

TEST_CASE("every block has a map colour; only air is see-through") {
  CHECK(mapColour(BlockType::AIR).a == 0);
  for (int v = 1; v <= static_cast<int>(kLastBlockType); ++v) {
    CAPTURE(v);
    CHECK(mapColour(static_cast<BlockType>(v)).a == 255);
  }
  // The ones a player steers by look different.
  CHECK(mapColour(BlockType::GRASS) != mapColour(BlockType::WATER));
  CHECK(mapColour(BlockType::SAND) != mapColour(BlockType::STONE));
  CHECK(mapColour(BlockType::WATER).b > mapColour(BlockType::WATER).r);
  CHECK(mapColour(BlockType::GRASS).g > mapColour(BlockType::GRASS).r);
  CHECK(mapColour(bedBlock(BedColour::Red, false, DoorFacing::North)) !=
        mapColour(bedBlock(BedColour::Blue, true, DoorFacing::East)));
}

TEST_CASE("the map turns so that the way you face is up") {
  auto near = [](glm::vec2 a, glm::vec2 b) { return glm::length(a - b) < 1e-4f; };
  // Facing north (-Z): north is up and east is right.
  CHECK(near(mapOffset({0, -5}, -90.0f), {0, -5}));
  CHECK(near(mapOffset({5, 0}, -90.0f), {5, 0}));
  // Facing east (+X): east is up and south is right.
  CHECK(near(mapOffset({5, 0}, 0.0f), {0, -5}));
  CHECK(near(mapOffset({0, 5}, 0.0f), {5, 0}));
  // Facing south: north is down.
  CHECK(near(mapOffset({0, -5}, 90.0f), {0, 5}));
  // Turning never stretches it.
  for (float yaw : {-90.0f, 13.0f, 200.0f})
    CHECK(glm::length(mapOffset({3, 4}, yaw)) == doctest::Approx(5.0f));
}

TEST_CASE("a marker beyond the rim sits on it, pointing the way") {
  MarkerSpot in = placeMarker({3, -4}, 10.0f);
  CHECK_FALSE(in.on_rim);
  CHECK(in.at == glm::vec2(3, -4));

  MarkerSpot far = placeMarker({30, -40}, 10.0f);
  CHECK(far.on_rim);
  CHECK(glm::length(far.at) == doctest::Approx(10.0f));
  CHECK(far.at.x == doctest::Approx(6.0f));
  CHECK(far.at.y == doctest::Approx(-8.0f));
}

TEST_CASE_FIXTURE(MapRig, "under rock is where the sky's light doesn't reach") {
  CHECK(underRock(world, inTunnel()));
  CHECK_FALSE(underRock(world, onTop()));
  // Nor is a chunk that hasn't loaded.
  CHECK_FALSE(underRock(world, {-20.5f, 50.0f, 8.5f}));
  // Swimming isn't caving.
  fill(world, {2, 50, 8}, {13, 51, 8}, BlockType::WATER);
  CHECK_FALSE(underRock(world, inTunnel()));
}

TEST_CASE_FIXTURE(MapRig, "the surface view shows each column's top block, lit by its height") {
  // A red pillar at (10, 10), five blocks above the stone.
  fill(world, {10, kTop + 1, 10}, {10, kTop + 5, 10}, BlockType::RED_BLOCK);
  Minimap map;
  map.sample(world, glm::ivec3(onTop()), Minimap::View::Surface);
  CHECK(map.view() == Minimap::View::Surface);
  CHECK(map.centre() == glm::ivec3(8, kTop + 1, 8));

  // Flat stone, unshaded.
  CHECK(map.cell(-3, -3) == mapColour(BlockType::STONE));
  // The pillar's top, lit: it stands above its north-west neighbour.
  const glm::u8vec4 pillar = map.cell(2, 2);
  CHECK(pillar.r > mapColour(BlockType::RED_BLOCK).r);
  CHECK(pillar.r > pillar.g);
  // The stone south-east of it is lower than the pillar: shaded.
  CHECK(brightness(map.cell(3, 3)) < brightness(mapColour(BlockType::STONE)));
  // Chunks that aren't loaded are left empty, and off the map is nothing.
  CHECK(map.cell(-20, 0).a == 0);
  CHECK(map.cell(Minimap::kRadius + 1, 0).a == 0);
}

TEST_CASE_FIXTURE(MapRig, "the cave view is a slice at the feet: open space light, rock dark") {
  fill(world, {12, 50, 8}, {12, 50, 8}, BlockType::WATER);
  // A step up at (3, 9): room to stand one block higher.
  fill(world, {3, 51, 9}, {3, 52, 9}, BlockType::AIR);
  Minimap map;
  map.sample(world, glm::ivec3(inTunnel()), Minimap::View::Cave);

  const glm::u8vec4 rock = map.cell(0, 3);
  const glm::u8vec4 open = map.cell(0, 0);
  CHECK(brightness(open) > brightness(rock) + 300);
  CHECK(map.cell(5, 0) == open);   // along the tunnel
  CHECK(map.cell(10, 0) == rock);  // past its end
  CHECK(map.cell(0, -1) == rock);  // beside it
  const glm::u8vec4 water = map.cell(8, 0);
  CHECK(water.b > water.r);
  CHECK(water != open);
  // The step shows as open, a little darker.
  const glm::u8vec4 step = map.cell(-1, 1);
  CHECK(brightness(step) < brightness(open));
  CHECK(brightness(step) > brightness(rock) + 300);
}

TEST_CASE_FIXTURE(MapRig, "the map waits a moment before switching view") {
  Minimap map;
  map.update(world, onTop(), 0.1f);
  CHECK(map.view() == Minimap::View::Surface);

  // Into the tunnel: still the surface until the cave has held a while.
  map.update(world, inTunnel(), 0.1f);
  map.update(world, inTunnel(), 0.5f);
  CHECK(map.view() == Minimap::View::Surface);
  // A moment back outside starts the wait over.
  map.update(world, onTop(), 0.1f);
  map.update(world, inTunnel(), 0.5f);
  CHECK(map.view() == Minimap::View::Surface);
  map.update(world, inTunnel(), 0.3f);
  CHECK(map.view() == Minimap::View::Cave);
  CHECK(map.centre() == glm::ivec3(inTunnel()));
  CHECK(brightness(map.cell(0, 0)) > brightness(map.cell(0, 3)) + 300);

  map.update(world, onTop(), Minimap::kSwitchSeconds);
  CHECK(map.view() == Minimap::View::Surface);
}

TEST_CASE_FIXTURE(MapRig, "the picture follows the player and catches up with changes") {
  Minimap map;
  map.update(world, onTop(), 0.1f);
  CHECK(map.cell(0, 0) == mapColour(BlockType::STONE));
  // A block placed underfoot shows by the next refresh, without moving.
  fill(world, {8, kTop + 1, 8}, {8, kTop + 1, 8}, BlockType::GRASS);
  map.update(world, onTop(), 0.1f);
  CHECK(map.cell(0, 0) == mapColour(BlockType::STONE));
  map.update(world, onTop(), Minimap::kRefreshSeconds);
  CHECK(map.cell(0, 0) != mapColour(BlockType::STONE));
  // Moving a block resamples at once, around the new spot.
  map.update(world, onTop() + glm::vec3(1, 0, 0), 0.01f);
  CHECK(map.centre() == glm::ivec3(9, kTop + 1, 8));
  CHECK(map.cell(-1, 0) != mapColour(BlockType::STONE));
}
