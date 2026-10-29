#include "doctest/doctest.h"
#include "entity/boat.hpp"
#include "entity/entity_store.hpp"
#include "physics/physics_body.hpp"
#include "item/item.hpp"
#include "item/recipes.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

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

// ─── The item, recipe and entity ───────────────────────────────────────

TEST_CASE("the Boat item and entity are pinned") {
  CHECK(static_cast<int>(EntityKind::Boat) == 16);
  CHECK(isValidItem(ItemId::BOAT));
  CHECK(itemDef(ItemId::BOAT).name == "Boat");
  CHECK(itemDef(ItemId::BOAT).max_stack == 1);
  CHECK_FALSE(itemDef(ItemId::BOAT).places); // the Game puts a boat entity down
  const auto &palette = paletteItems();
  CHECK(std::find(palette.begin(), palette.end(), ItemId::BOAT) != palette.end());
}

TEST_CASE("5 planks of any wood make a boat, anywhere") {
  const Recipe *boat = recipeFor(ItemId::BOAT);
  REQUIRE(boat);
  CHECK(boat->makes == ItemStack{ItemId::BOAT, 1});
  CHECK_FALSE(boat->needs_table);
  REQUIRE(boat->needs.size() == 1);
  CHECK(boat->needs[0].count == 5);
  // A survival player gets planks from any tree's logs.
  for (BlockType planks : {BlockType::OAK_PLANKS, BlockType::PINE_PLANKS,
                           BlockType::PALM_PLANKS, BlockType::CHERRY_PLANKS}) {
    CAPTURE(static_cast<int>(planks));
    const auto &any = boat->needs[0].any_of;
    CHECK(std::find(any.begin(), any.end(), itemForBlock(planks)) != any.end());
  }
}

TEST_CASE("a boat is a vehicle that can't be hurt") {
  const EntityTypeDef &def = entityType(EntityKind::Boat);
  CHECK(def.vehicle);
  CHECK(isVehicle(EntityKind::Boat));
  CHECK_FALSE(isVehicle(EntityKind::Woolly));
  CHECK(def.max_health == 0);
  CHECK(def.despawn_age_ms == 0);
  CHECK_FALSE(def.brain);
  CHECK_FALSE(def.model.empty());
  // Narrow enough for a one-block canal.
  CHECK(def.half_width * 2.0f < 1.0f);
}

TEST_CASE("boats stay put: never despawned, never moved by the store") {
  ScopedRenderDistance rd{0};
  World world;
  world.loadChunkSync(0, 0);
  EntityStore store;
  Rng rng{7};
  // Floating in the sky: anything stepped by physics would fall.
  const glm::vec3 at{8.0f, 200.0f, 8.0f};
  auto id = store.spawn(EntityKind::Boat, at);
  REQUIRE(id);
  // Every player is far beyond the despawn distance.
  std::vector<glm::vec3> far{{8.0f + EntityStore::kDespawnDistance * 3, 200.0f, 8.0f}};
  for (int i = 0; i < 20 * 60 * 10; ++i) store.tick(world, rng, far); // ten minutes
  REQUIRE(store.find(*id));
  CHECK(store.find(*id)->body.position == at);
}

// ─── Floating and steering ─────────────────────────────────────────────

namespace {

constexpr int kBase = 200; // the rig's stone top: clear sky above

// A long canal: stone from x 0 to 47, with water 3 deep in x 2..45, z 5..10
// (its top at kBase + 1, level with the stone banks).
struct Canal {
  ScopedRenderDistance rd{0};
  World world;
  BoatState boat;
  static constexpr float kSurface = kBase + 1.0f;

  Canal() {
    for (int cx = 0; cx < 3; ++cx) world.loadChunkSync(cx, 0);
    fill(world, {0, kBase - 3, 0}, {47, kBase, 15}, BlockType::STONE);
    fill(world, {2, kBase - 2, 5}, {45, kBase, 10}, BlockType::WATER);
    fill(world, {0, kBase + 1, 0}, {47, kBase + 8, 15}, BlockType::AIR);
    boat.position = {6.5f, floatHeight(kSurface), 8.0f};
    boat.yaw = 90.0f; // facing +X, along the canal
  }

  // Drives for `seconds` in frames of `dt`.
  void drive(float seconds, BoatInput in, float dt = 1.0f / 60.0f) {
    for (float t = 0.0f; t < seconds; t += dt) steerBoat(boat, in, world, dt);
  }
  static BoatInput ahead(float look_yaw = 90.0f) { return {true, false, look_yaw}; }
  bool inBlock() const {
    PhysicsBody body;
    body.position = boat.position;
    body.half_width = entityType(EntityKind::Boat).half_width;
    body.height = entityType(EntityKind::Boat).height;
    return body.collidesAt(&world, boat.position);
  }
};

} // namespace

TEST_CASE("forward and seats follow the yaw; a camera's yaw turns into a boat's") {
  CHECK(glm::length(boatForward(0.0f) - glm::vec3(0, 0, -1)) < 1e-5f);
  CHECK(glm::length(boatForward(90.0f) - glm::vec3(1, 0, 0)) < 1e-5f);
  CHECK(glm::length(boatForward(boatYawFor(0.0f)) - glm::vec3(1, 0, 0)) < 1e-5f);   // camera 0 = +X
  CHECK(glm::length(boatForward(boatYawFor(-90.0f)) - glm::vec3(0, 0, -1)) < 1e-5f); // camera -90 = -Z
  const glm::vec3 at{10.0f, 50.0f, 10.0f};
  glm::vec3 driver = seatPosition(at, 90.0f, 0), passenger = seatPosition(at, 90.0f, 1);
  CHECK(driver.x < at.x);    // behind
  CHECK(passenger.x > at.x); // in front
  CHECK(driver.y == doctest::Approx(at.y + kBoatSeatHeight));
}

TEST_CASE_FIXTURE(Canal, "a boat floats at the surface and sits still") {
  CHECK(afloat(world, boat.position));
  CHECK(waterSurfaceAt(world, boat.position) == kSurface);
  const glm::vec3 start = boat.position;
  drive(2.0f, {});
  CHECK(boat.position == start);
  CHECK(boat.speed == 0.0f);
}

TEST_CASE_FIXTURE(Canal, "W speeds up to 8 blocks a second and no more") {
  drive(0.5f, ahead());
  CHECK(boat.speed > 3.0f);
  CHECK(boat.speed < kBoatMaxSpeed);
  drive(1.0f, ahead());
  CHECK(boat.speed == doctest::Approx(kBoatMaxSpeed));
  float x = boat.position.x;
  drive(1.0f, ahead());
  CHECK(boat.position.x - x == doctest::Approx(kBoatMaxSpeed).epsilon(0.02));
  CHECK(boat.position.y == floatHeight(kSurface)); // still floating
}

TEST_CASE_FIXTURE(Canal, "with no keys it coasts to a stop in about a second") {
  drive(1.5f, ahead());
  REQUIRE(boat.speed == doctest::Approx(kBoatMaxSpeed));
  drive(1.1f, {});
  CHECK(boat.speed == 0.0f);
}

TEST_CASE_FIXTURE(Canal, "S slows down, then backs up slowly") {
  boat.position.x = 30.0f;
  drive(1.5f, ahead());
  drive(0.5f, {false, true, 90.0f});
  CHECK(boat.speed > 0.0f);
  CHECK(boat.speed < kBoatMaxSpeed);
  drive(3.0f, {false, true, 90.0f});
  CHECK(boat.speed == doctest::Approx(-kBoatReverseSpeed));
}

TEST_CASE_FIXTURE(Canal, "it turns toward the look, no faster than its turn speed") {
  drive(0.25f, ahead(180.0f));
  CHECK(boat.yaw > 90.0f + 20.0f);
  CHECK(boat.yaw <= 90.0f + kBoatTurnSpeed * 0.26f);
  drive(2.0f, ahead(180.0f));
  CHECK(boat.yaw == doctest::Approx(180.0f));
  // The short way round: from 170 to -170 is 20 degrees, not 340.
  boat.yaw = 170.0f;
  drive(0.1f, ahead(-170.0f));
  CHECK(std::fmod(boat.yaw + 360.0f, 360.0f) > 170.0f);
}

TEST_CASE_FIXTURE(Canal, "looking around without W doesn't turn it") {
  drive(1.0f, {false, false, 0.0f});
  CHECK(boat.yaw == 90.0f);
  drive(1.0f, {false, true, 0.0f});
  CHECK(boat.yaw == 90.0f);
}

TEST_CASE_FIXTURE(Canal, "it stops at the bank instead of going into it") {
  boat.position.x = 38.0f;
  drive(3.0f, ahead()); // straight at the end of the canal (x 46)
  CHECK(boat.speed == 0.0f);
  CHECK(boat.position.x < 46.0f);
  CHECK(boat.position.x > 44.0f);
  CHECK_FALSE(inBlock());
  CHECK(afloat(world, boat.position)); // and it didn't climb out
}

TEST_CASE_FIXTURE(Canal, "a long frame still stops at the bank") {
  boat.position.x = 40.0f;
  boat.speed = kBoatMaxSpeed;
  drive(1.0f, ahead(), 0.5f);
  CHECK(boat.position.x < 46.0f);
  CHECK_FALSE(inBlock());
}

TEST_CASE_FIXTURE(Canal, "on land it has no power") {
  boat.position = {20.5f, kBase + 1.0f, 2.5f}; // on the stone beside the canal
  drive(2.0f, ahead());
  CHECK(boat.position == glm::vec3(20.5f, kBase + 1.0f, 2.5f));
  CHECK_FALSE(afloat(world, boat.position));
}

TEST_CASE_FIXTURE(Canal, "a beached boat that's moving stops quickly") {
  boat.position = {20.5f, kBase + 1.0f, 2.5f};
  boat.speed = kBoatMaxSpeed;
  drive(0.5f, {});
  CHECK(boat.speed == 0.0f);
}

TEST_CASE_FIXTURE(Canal, "off the water it falls, onto land or back onto the water") {
  boat.position = {20.5f, kBase + 6.0f, 2.5f};
  drive(2.0f, {});
  CHECK(std::abs(boat.position.y - (kBase + 1.0f)) < 0.05f); // resting on the stone
  boat.position = {20.5f, kBase + 30.0f, 8.0f}; // high above the canal: it lands fast
  drive(3.0f, {});
  CHECK(boat.position.y == floatHeight(kSurface));
  CHECK(afloat(world, boat.position));
}

TEST_CASE_FIXTURE(Canal, "in unloaded ground it holds still") {
  boat.position = {200.5f, kBase + 6.0f, 8.0f}; // chunk 12: never loaded
  boat.speed = 5.0f;
  drive(1.0f, ahead());
  CHECK(boat.position == glm::vec3(200.5f, kBase + 6.0f, 8.0f));
}

TEST_CASE_FIXTURE(Canal, "the Game takes moves along the water, and nothing else") {
  const glm::vec3 from = boat.position;
  CHECK(boatMoveAllowed(world, from, from + glm::vec3(0.2f, 0, 0)));
  CHECK(boatMoveAllowed(world, from, from + glm::vec3(2.5f, 0, 0))); // a slow frame
  CHECK_FALSE(boatMoveAllowed(world, from, from + glm::vec3(8.0f, 0, 0)));  // too far
  CHECK_FALSE(boatMoveAllowed(world, from, {20.5f, kBase + 1.0f, 2.5f}));   // onto land
  CHECK_FALSE(boatMoveAllowed(world, from, from + glm::vec3(0, 3.0f, 0)));  // flying up
  const float nan = std::nanf("");
  CHECK_FALSE(boatMoveAllowed(world, from, {nan, from.y, from.z}));
  // A beached boat may settle.
  const glm::vec3 high{20.5f, kBase + 3.0f, 2.5f};
  CHECK(boatMoveAllowed(world, high, high - glm::vec3(0, 1.0f, 0)));
  CHECK_FALSE(boatMoveAllowed(world, high, high + glm::vec3(1.0f, -1.0f, 0)));
}
