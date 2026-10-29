#include "doctest/doctest.h"
#include "entity/boat.hpp"
#include "entity/entity_interp.hpp"
#include "entity/entity_store.hpp"
#include "entity/player_model.hpp"
#include "physics/physics_body.hpp"
#include "block/block_data.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "world/world_save.hpp"
#include <filesystem>
#include <fstream>
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

TEST_CASE_FIXTURE(Canal, "it won't sail off the edge of the water over a hole") {
  fill(world, {46, kBase - 3, 5}, {47, kBase, 10}, BlockType::AIR); // dug out past the end
  boat.position.x = 38.0f;
  drive(3.0f, ahead());
  CHECK(boat.speed == 0.0f);
  CHECK(afloat(world, boat.position));
  CHECK(boat.position.x < 46.0f);
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

// ─── In the Game ───────────────────────────────────────────────────────

namespace {

std::vector<Event> pollAll(LocalConnection &conn) {
  std::vector<Event> out;
  conn.poll(out);
  return out;
}

template <class T> std::optional<T> find(const std::vector<Event> &evs) {
  for (const Event &e : evs)
    if (const T *t = std::get_if<T>(&e)) return *t;
  return std::nullopt;
}

bool rejected(const std::vector<Event> &evs) { return find<CommandRejected>(evs).has_value(); }

// A harbour in the sky over chunk (0,0): stone from x/z 0 to 15 with a pool
// of water 3 deep in x/z 4..11 (its top at kBase + 1, level with the
// stone). Three players stand on the bank west of it, looking east.
struct Harbour {
  ScopedRenderDistance rd{0};
  Game game;
  LocalConnection a, b, c;
  PlayerId ida = 0, idb = 0, idc = 0;
  static constexpr float kSurface = kBase + 1.0f;
  static inline const glm::vec3 kBank{2.5f, kBase + 1.0f, 7.5f};
  static inline const glm::ivec3 kWater{6, kBase, 7}; // the pool's top, two blocks out

  Harbour() {
    game.spawner() = Spawner{};
    ida = join(a);
    idb = join(b);
    idc = join(c);
    stand(a, ida, kBank);
    stand(b, idb, kBank);
    stand(c, idc, kBank);
    game.world().loadChunkSync(0, 0);
    fill(game.world(), {0, kBase - 3, 0}, {15, kBase, 15}, BlockType::STONE);
    fill(game.world(), {4, kBase - 2, 4}, {11, kBase, 11}, BlockType::WATER);
    fill(game.world(), {0, kBase + 1, 0}, {15, kBase + 10, 15}, BlockType::AIR);
  }

  PlayerId join(LocalConnection &conn) {
    game.addClient(conn);
    conn.send(Join{});
    game.pump();
    auto joined = find<Joined>(pollAll(conn));
    REQUIRE(joined);
    return joined->id;
  }

  PlayerState &ps(PlayerId id) { return *game.findPlayer(id); }

  std::vector<Event> run(LocalConnection &conn, Command cmd) {
    conn.send(std::move(cmd));
    game.pump();
    return pollAll(conn);
  }

  MovePlayer move(PlayerId id, glm::vec3 at, float yaw = 0.0f) {
    MovePlayer m;
    m.position = at;
    m.yaw = yaw;
    m.respawn = ps(id).respawn;
    return m;
  }
  void stand(LocalConnection &conn, PlayerId id, glm::vec3 at, float yaw = 0.0f) {
    run(conn, move(id, at, yaw));
  }

  void hold(PlayerId id, ItemId item) {
    ps(id).inventory.set(ps(id).selected_slot, {item, 1});
  }

  std::vector<const Entity *> boats() const {
    std::vector<const Entity *> out;
    for (const auto &[id, e] : game.entities().all())
      if (e.kind == EntityKind::Boat) out.push_back(&e);
    return out;
  }

  // Player a puts a boat on the pool two blocks out from the bank.
  EntityId launch() {
    hold(ida, ItemId::BOAT);
    auto evs = run(a, UseItemOn{kWater});
    REQUIRE_FALSE(rejected(evs));
    REQUIRE(boats().size() == 1);
    return boats()[0]->id;
  }

  // Steps a player up beside the boat and gets them in.
  std::vector<Event> board(LocalConnection &conn, PlayerId id, EntityId boat) {
    stand(conn, id, {3.6f, kBase + 1.0f, 7.5f});
    return run(conn, BoardBoat{boat});
  }

  int count(PlayerId id, ItemId item) {
    int n = 0;
    for (const ItemStack &s : ps(id).inventory.slots()) n += s.id == item ? s.count : 0;
    return n;
  }
};

} // namespace

TEST_CASE_FIXTURE(Harbour, "a boat goes on the water's top, facing the way you look, and is used up") {
  hold(ida, ItemId::BOAT);
  auto evs = run(a, UseItemOn{kWater});
  CHECK_FALSE(rejected(evs));
  REQUIRE(boats().size() == 1);
  const Entity &boat = *boats()[0];
  CHECK(boat.body.position == glm::vec3(6.5f, floatHeight(kSurface), 7.5f));
  CHECK(boat.yaw == doctest::Approx(90.0f)); // looking east (+X)
  CHECK(count(ida, ItemId::BOAT) == 0);
  CHECK(find<InventoryContents>(evs));
}

TEST_CASE_FIXTURE(Harbour, "aimed at the water below its top, the boat still goes on top") {
  hold(ida, ItemId::BOAT);
  CHECK_FALSE(rejected(run(a, UseItemOn{kWater - glm::ivec3(0, 1, 0)})));
  REQUIRE(boats().size() == 1);
  CHECK(boats()[0]->body.position.y == floatHeight(kSurface));
}

TEST_CASE_FIXTURE(Harbour, "boats don't go on land, on a boat, or on a player") {
  hold(ida, ItemId::BOAT);
  CHECK(rejected(run(a, UseItemOn{{3, kBase, 7}}))); // the stone bank
  CHECK(boats().empty());
  CHECK(count(ida, ItemId::BOAT) == 1);

  launch();
  hold(ida, ItemId::BOAT);
  CHECK(rejected(run(a, UseItemOn{kWater})));
  CHECK(boats().size() == 1);

  stand(b, idb, {7.5f, kBase + 0.4f, 9.5f}); // swimming there
  CHECK(rejected(run(a, UseItemOn{{7, kBase, 9}})));
  CHECK(boats().size() == 1);
  CHECK(count(ida, ItemId::BOAT) == 1);
}

TEST_CASE_FIXTURE(Harbour, "in creative a boat isn't used up") {
  game.setMode(GameMode::Creative);
  launch();
  CHECK(count(ida, ItemId::BOAT) == 1);
}

TEST_CASE_FIXTURE(Harbour, "the first in drives, the second rides in front, and a third can't get in") {
  EntityId boat = launch();
  auto evs = board(a, ida, boat);
  auto riding = find<Riding>(evs);
  REQUIRE(riding);
  CHECK(riding->boat == boat);
  CHECK(riding->seat == 0);
  CHECK(riding->position == boats()[0]->body.position);
  auto sat = find<Respawned>(evs);
  REQUIRE(sat);
  const glm::vec3 driver_seat = seatPosition(boats()[0]->body.position, boats()[0]->yaw, 0);
  CHECK(sat->position == driver_seat);
  CHECK(ps(ida).position == driver_seat);

  auto evs_b = board(b, idb, boat);
  REQUIRE(find<Riding>(evs_b));
  CHECK(find<Riding>(evs_b)->seat == 1);
  CHECK(ps(idb).position == seatPosition(boats()[0]->body.position, boats()[0]->yaw, 1));

  auto evs_c = board(c, idc, boat);
  CHECK(rejected(evs_c));
  CHECK_FALSE(ps(idc).riding);
}

TEST_CASE_FIXTURE(Harbour, "a boat out of reach, or something that isn't a boat, can't be boarded") {
  EntityId boat = launch();
  stand(a, ida, {14.5f, kBase + 1.0f, 14.5f});
  CHECK(rejected(run(a, BoardBoat{boat})));
  CHECK(rejected(run(a, BoardBoat{boat + 100})));
  CHECK_FALSE(ps(ida).riding);
}

TEST_CASE_FIXTURE(Harbour, "getting out puts you on the bank, never in a block") {
  EntityId boat = launch();
  board(a, ida, boat);
  auto evs = run(a, LeaveBoat{});
  auto riding = find<Riding>(evs);
  REQUIRE(riding);
  CHECK(riding->boat == 0);
  auto stood = find<Respawned>(evs);
  REQUIRE(stood);
  CHECK_FALSE(ps(ida).riding);
  CHECK(ps(ida).position == stood->position);
  // Dry land: standing on the stone, at the waterline.
  CHECK(stood->position.y == doctest::Approx(kBase + 1.0f));
  CHECK(glm::distance(stood->position, glm::vec3(6.5f, kBase + 1.0f, 7.5f)) <= 3.0f);
  const World &w = game.world();
  CHECK_FALSE(blocksMovement(w.getBlockAt(stood->position + glm::vec3(0, 0.5f, 0))));
  CHECK_FALSE(blocksMovement(w.getBlockAt(stood->position + glm::vec3(0, 1.5f, 0))));
  CHECK(blocksMovement(w.getBlockAt(stood->position - glm::vec3(0, 0.5f, 0))));
}

TEST_CASE_FIXTURE(Harbour, "far from land, getting out puts you in the water beside the boat") {
  EntityId boat = launch();
  board(a, ida, boat);
  // Drive to the middle of a bigger pool.
  fill(game.world(), {4, kBase - 2, 4}, {14, kBase, 14}, BlockType::WATER);
  for (glm::vec3 to : {glm::vec3(8.0f, floatHeight(kSurface), 8.5f),
                       glm::vec3(9.5f, floatHeight(kSurface), 9.5f)}) { // in two short moves
    MovePlayer m = move(ida, ps(ida).position);
    m.boat = boat;
    m.boat_position = to;
    m.boat_yaw = 90.0f;
    CHECK_FALSE(find<Riding>(run(a, m)));
  }
  REQUIRE(boats()[0]->body.position.x == 9.5f);
  auto stood = find<Respawned>(run(a, LeaveBoat{}));
  REQUIRE(stood);
  CHECK(isLiquid(game.world().getBlockAt(stood->position + glm::vec3(0, 0.1f, 0))));
  CHECK(game.world().getBlockAt(stood->position + glm::vec3(0, 1.0f, 0)) == BlockType::AIR);
}

TEST_CASE_FIXTURE(Harbour, "the driver steers the boat, and the passenger moves with it") {
  EntityId boat = launch();
  board(a, ida, boat);
  board(b, idb, boat);
  const glm::vec3 from = boats()[0]->body.position;
  MovePlayer m = move(ida, {0, 0, 0}); // a rider's own position is ignored
  m.boat = boat;
  m.boat_position = from + glm::vec3(0.0f, 0.0f, 1.5f);
  m.boat_yaw = 180.0f;
  auto evs = run(a, m);
  CHECK_FALSE(find<Riding>(evs)); // taken: nothing to put back
  CHECK(boats()[0]->body.position == m.boat_position);
  CHECK(boats()[0]->yaw == 180.0f);
  CHECK(ps(ida).position == seatPosition(m.boat_position, 180.0f, 0));
  CHECK(ps(idb).position == seatPosition(m.boat_position, 180.0f, 1));
}

TEST_CASE_FIXTURE(Harbour, "a move too far or onto land is refused, and the boat is put back") {
  EntityId boat = launch();
  board(a, ida, boat);
  const glm::vec3 from = boats()[0]->body.position;
  for (glm::vec3 to : {from + glm::vec3(0, 0, 4.0f),               // too far at once
                       glm::vec3(2.5f, kBase + 1.0f, 7.5f)}) {     // up onto the bank
    MovePlayer m = move(ida, {0, 0, 0});
    m.boat = boat;
    m.boat_position = to;
    auto riding = find<Riding>(run(a, m));
    REQUIRE(riding);
    CHECK(riding->position == from);
    CHECK(boats()[0]->body.position == from);
  }
}

TEST_CASE_FIXTURE(Harbour, "only the driver steers") {
  EntityId boat = launch();
  board(a, ida, boat);
  board(b, idb, boat);
  const glm::vec3 from = boats()[0]->body.position;
  MovePlayer m = move(idb, {0, 0, 0});
  m.boat = boat;
  m.boat_position = from + glm::vec3(0.0f, 0.0f, 1.0f);
  run(b, m);
  CHECK(boats()[0]->body.position == from);
}

TEST_CASE_FIXTURE(Harbour, "when the driver gets out, the passenger stays a passenger") {
  EntityId boat = launch();
  board(a, ida, boat);
  board(b, idb, boat);
  run(a, LeaveBoat{});
  REQUIRE(ps(idb).riding);
  CHECK(ps(idb).riding->seat == 1);
  // The next one in drives.
  auto riding = find<Riding>(board(c, idc, boat));
  REQUIRE(riding);
  CHECK(riding->seat == 0);
}

TEST_CASE_FIXTURE(Harbour, "riders can't break, place, use or hit, but can fish") {
  EntityId boat = launch();
  board(a, ida, boat);
  CHECK(rejected(run(a, BreakBlock{{3, kBase, 7}})));
  hold(ida, itemForBlock(BlockType::STONE));
  CHECK(rejected(run(a, PlaceBlock{{3, kBase + 1, 6}})));
  hold(ida, ItemId::BOAT);
  CHECK(rejected(run(a, UseItemOn{{8, kBase, 9}})));
  stand(b, idb, {5.5f, kBase + 1.0f, 7.5f});
  CHECK(rejected(run(a, AttackEntity{boat}))); // nor pick their own boat up

  hold(ida, ItemId::FISHING_ROD);
  CHECK_FALSE(rejected(run(a, CastLine{{1.0f, 0.0f, 0.0f}})));
  REQUIRE(ps(ida).fishing);
  const Entity *bobber = game.entities().find(ps(ida).fishing->bobber);
  REQUIRE(bobber);
  // Cast from a seated rider's eyes, not a standing player's.
  CHECK(bobber->body.position.y < ps(ida).position.y + kSeatedEyeHeight + 0.1f);
}

TEST_CASE_FIXTURE(Harbour, "an empty boat is picked up by a hit; one with someone in it isn't") {
  EntityId boat = launch();
  board(b, idb, boat);
  stand(a, ida, {3.6f, kBase + 1.0f, 7.5f});
  CHECK(rejected(run(a, AttackEntity{boat})));
  CHECK(boats().size() == 1);

  run(b, LeaveBoat{});
  auto evs = run(a, AttackEntity{boat});
  CHECK_FALSE(rejected(evs));
  CHECK(boats().empty());
  CHECK(count(ida, ItemId::BOAT) == 1);
  CHECK(find<InventoryContents>(evs));
}

TEST_CASE_FIXTURE(Harbour, "with a full inventory, a picked-up boat drops beside you") {
  EntityId boat = launch();
  for (int i = 0; i < kInventorySlots; ++i) ps(ida).inventory.set(i, {ItemId::SOGGY_BOOT, 1});
  stand(a, ida, {3.6f, kBase + 1.0f, 7.5f});
  run(a, AttackEntity{boat});
  CHECK(boats().empty());
  int dropped = 0;
  for (const auto &[id, e] : game.entities().all())
    dropped += e.kind == EntityKind::DroppedItem && e.item.id == ItemId::BOAT;
  CHECK(dropped == 1);
}

TEST_CASE_FIXTURE(Harbour, "dying or leaving the game gets you out") {
  EntityId boat = launch();
  board(a, ida, boat);
  board(b, idb, boat);
  ps(ida).vitals.damage(1000);
  game.tick();
  CHECK_FALSE(ps(ida).riding);
  auto riding = find<Riding>(pollAll(a));
  REQUIRE(riding);
  CHECK(riding->boat == 0);

  game.removeClient(b);
  // Both seats are free again.
  CHECK(find<Riding>(board(c, idc, boat))->seat == 0);
}

TEST_CASE_FIXTURE(Harbour, "nothing can be built inside a boat") {
  launch();
  game.setMode(GameMode::Creative);
  hold(idb, itemForBlock(BlockType::STONE));
  stand(b, idb, {3.6f, kBase + 1.0f, 7.5f});
  CHECK(rejected(run(b, PlaceBlock{{6, kBase + 1, 7}})));
  CHECK(game.world().getBlockAt({6.5f, kBase + 1.5f, 7.5f}) == BlockType::AIR);
}

TEST_CASE_FIXTURE(Harbour, "others see who's sitting in which boat") {
  EntityId boat = launch();
  board(b, idb, boat);
  pollAll(a);
  game.tick();
  std::optional<PlayerView> view;
  for (const Event &e : pollAll(a))
    if (const auto *snap = std::get_if<EntitySnapshot>(&e))
      for (const PlayerView &p : snap->players)
        if (p.id == idb) view = p;
  REQUIRE(view);
  CHECK(view->riding == boat);
  CHECK(view->seat == 0);
}

TEST_CASE_FIXTURE(Harbour, "a rider can't go to bed") {
  EntityId boat = launch();
  board(a, ida, boat);
  CHECK(rejected(run(a, UseBed{{3, kBase, 7}})));
}

// ─── Saving ────────────────────────────────────────────────────────────

TEST_CASE("boats.dat round-trips, and a missing or damaged one loads nothing") {
  TempDir dir;
  const std::string d = dir.path.string();
  std::vector<WorldSave::BoatRecord> none;
  CHECK_FALSE(WorldSave::loadBoats(d, none)); // a world from before boats

  const std::vector<WorldSave::BoatRecord> boats{{{1.5f, 62.85f, -40.5f}, 90.0f},
                                                 {{-300.5f, 10.85f, 7.5f}, 271.5f}};
  REQUIRE(WorldSave::saveBoats(d, boats));
  std::vector<WorldSave::BoatRecord> back;
  REQUIRE(WorldSave::loadBoats(d, back));
  REQUIRE(back.size() == 2);
  CHECK(back[1].position == boats[1].position);
  CHECK(back[1].yaw == boats[1].yaw);

  // Cut short.
  std::filesystem::resize_file(dir.path / "boats.dat", 20);
  std::vector<WorldSave::BoatRecord> cut;
  CHECK_FALSE(WorldSave::loadBoats(d, cut));
  CHECK(cut.empty());
}

TEST_CASE_FIXTURE(Harbour, "boats are saved with the world, and riders are saved standing beside them") {
  ScopedSeed seed;
  TempDir dir;
  game.load(dir.path.string()); // binds the save directory (nothing there yet)
  EntityId boat = launch();
  board(a, ida, boat); // the host, in the boat
  const glm::vec3 at = boats()[0]->body.position;
  const float yaw = boats()[0]->yaw;
  REQUIRE(game.save());

  WorldSave::PlayerData host;
  REQUIRE(WorldSave::loadPlayer(dir.path.string(), host));
  CHECK(host.position.y == doctest::Approx(kBase + 1.0f)); // on the bank, not in the seat
  CHECK(host.position != ps(ida).position);

  Game loaded;
  loaded.spawner() = Spawner{};
  REQUIRE(loaded.load(dir.path.string()));
  int n = 0;
  for (const auto &[id, e] : loaded.entities().all())
    if (e.kind == EntityKind::Boat) {
      ++n;
      CHECK(e.body.position == at);
      CHECK(e.yaw == yaw);
    }
  CHECK(n == 1);
}

TEST_CASE("a world from before boats loads with none, and a damaged boats.dat is set aside") {
  ScopedSeed seed;
  ScopedRenderDistance rd{0};
  TempDir dir;
  {
    Game game;
    game.load(dir.path.string());
    REQUIRE(game.save());
  }
  std::filesystem::remove(dir.path / "boats.dat"); // as an older version left it
  {
    Game game;
    REQUIRE(game.load(dir.path.string()));
    CHECK(game.entities().size() == 0);
    CHECK_FALSE(std::filesystem::exists(dir.path / "boats.dat.unreadable"));
  }
  {
    std::ofstream junk(dir.path / "boats.dat", std::ios::binary);
    junk << "not a boat file";
  }
  Game game;
  REQUIRE(game.load(dir.path.string())); // the rest of the world still loads
  CHECK(game.entities().size() == 0);
  CHECK(std::filesystem::exists(dir.path / "boats.dat.unreadable"));
}

// ─── Drawing riders ────────────────────────────────────────────────────

TEST_CASE("a sitting figure fits in a boat: legs out in front, nothing swinging") {
  auto model = seatedPlayerModel(playerColor("Bea"));
  REQUIRE(model.size() == playerModel(playerColor("Bea")).size());
  float top = 0.0f, bottom = 1.0f, front = 0.0f;
  for (const ModelBox &b : model) {
    top = std::max(top, b.offset.y + b.size.y);
    bottom = std::min(bottom, b.offset.y);
    front = std::min(front, b.offset.z);
    CHECK(b.swing == 0);
  }
  CHECK(bottom == 0.0f);
  CHECK(top == doctest::Approx(kSeatedHeight).epsilon(0.05));
  CHECK(front < -0.4f); // the feet reach forward
  // The front seat's rider keeps their feet inside the bow.
  const EntityTypeDef &boat = entityType(EntityKind::Boat);
  float bow = 0.0f;
  for (const ModelBox &b : boat.model) bow = std::min(bow, b.offset.z);
  CHECK(-kBoatSeatOffset + front > bow);
}

TEST_CASE("riders are drawn in their seats on the boat as drawn, facing its way") {
  std::vector<EntityView> entities(2);
  entities[0].id = 5;
  entities[0].kind = EntityKind::Boat;
  entities[0].position = {10.0f, 60.85f, 4.0f};
  entities[0].yaw = 90.0f;
  entities[1].id = 6;
  entities[1].kind = EntityKind::Woolly;

  std::vector<PlayerView> players(3);
  players[0].riding = 5;
  players[0].seat = 1;
  players[0].position = {0.0f, 0.0f, 0.0f}; // a snapshot behind the boat
  players[0].moving = true;
  players[1].position = {3.0f, 70.0f, 3.0f}; // on foot
  players[2].riding = 99;                    // a boat out of sight
  players[2].position = {7.0f, 7.0f, 7.0f};

  placeRiders(players, entities);
  CHECK(players[0].position == seatPosition(entities[0].position, 90.0f, 1));
  CHECK(playerModelYaw(players[0].yaw) == doctest::Approx(90.0f));
  CHECK_FALSE(players[0].moving);
  CHECK(players[1].position == glm::vec3(3.0f, 70.0f, 3.0f));
  CHECK(players[2].position == glm::vec3(7.0f, 7.0f, 7.0f));
}

// ─── Loading back into the boat ────────────────────────────────────────

TEST_CASE("boats.dat keeps who sat where, and an older one without them still loads") {
  using Rider = WorldSave::BoatRecord::Rider;
  TempDir dir;
  const std::string d = dir.path.string();
  std::vector<WorldSave::BoatRecord> boats(2);
  boats[0].position = {1.5f, 62.85f, -40.5f};
  boats[0].seats = {{{Rider::Host, ""}, {Rider::Guest, "Sam"}}};
  boats[1].position = {9.5f, 62.85f, 3.5f};
  REQUIRE(WorldSave::saveBoats(d, boats));
  std::vector<WorldSave::BoatRecord> back;
  REQUIRE(WorldSave::loadBoats(d, back));
  REQUIRE(back.size() == 2);
  CHECK(back[0].seats == boats[0].seats);
  CHECK(back[1].seats == WorldSave::BoatRecord{}.seats);

  // As the first version with boats wrote it: the boats and nothing more.
  {
    std::ofstream o(dir.path / "boats.dat", std::ios::binary | std::ios::trunc);
    o.write("VXBO", 4);
    const uint32_t version = 1, n = 1;
    const float rec[4] = {1.5f, 62.85f, -40.5f, 90.0f};
    o.write(reinterpret_cast<const char *>(&version), 4);
    o.write(reinterpret_cast<const char *>(&n), 4);
    o.write(reinterpret_cast<const char *>(rec), sizeof rec);
  }
  std::vector<WorldSave::BoatRecord> old;
  REQUIRE(WorldSave::loadBoats(d, old));
  REQUIRE(old.size() == 1);
  CHECK(old[0].yaw == 90.0f);
  CHECK(old[0].seats == WorldSave::BoatRecord{}.seats);
}

namespace {

// A Harbour game saved with the host driving and b riding in front.
struct SavedAtSea : Harbour {
  ScopedSeed seed;
  TempDir dir;
  glm::vec3 boat_at{0.0f};
  float boat_yaw = 0.0f;
  std::string b_name;

  SavedAtSea() {
    game.load(dir.path.string());
    EntityId boat = launch();
    board(a, ida, boat);
    board(b, idb, boat);
    boat_at = boats()[0]->body.position;
    boat_yaw = boats()[0]->yaw;
    b_name = ps(idb).name;
    REQUIRE(game.save());
  }

  // The world loaded again, with its pool rebuilt where the boat floats.
  struct Reloaded {
    ScopedRenderDistance rd{0};
    Game game;
    explicit Reloaded(const std::string &dir) {
      game.spawner() = Spawner{};
      REQUIRE(game.load(dir));
    }
    EntityId boat() const {
      for (const auto &[id, e] : game.entities().all())
        if (e.kind == EntityKind::Boat) return id;
      FAIL("no boat");
      return 0;
    }
  };

  // Joins and returns everything the Game sent.
  static std::vector<Event> joinAs(Game &game, LocalConnection &conn, std::string name,
                                   Game::Role role) {
    game.addClient(conn, role);
    conn.send(Join{std::move(name)});
    game.pump();
    return pollAll(conn);
  }
};

} // namespace

TEST_CASE_FIXTURE(SavedAtSea, "the host loads back into the seat they saved in") {
  Reloaded world(dir.path.string());
  LocalConnection host;
  auto evs = joinAs(world.game, host, "", Game::Role::Host);
  auto riding = find<Riding>(evs);
  REQUIRE(riding);
  CHECK(riding->boat == world.boat());
  CHECK(riding->seat == 0);
  const PlayerState &me = *world.game.findPlayer(find<Joined>(evs)->id);
  REQUIRE(me.riding);
  CHECK(me.position == seatPosition(boat_at, boat_yaw, 0));
  auto sat = find<Respawned>(evs);
  REQUIRE(sat);
  CHECK(sat->position == me.position);
}

TEST_CASE_FIXTURE(SavedAtSea, "a guest loads back into their seat, and it stays saved") {
  Reloaded world(dir.path.string());
  LocalConnection host, guest;
  joinAs(world.game, host, "", Game::Role::Host);
  auto riding = find<Riding>(joinAs(world.game, guest, b_name, Game::Role::Guest));
  REQUIRE(riding);
  CHECK(riding->seat == 1);

  // A second save keeps them both in, for the next time.
  REQUIRE(world.game.save());
  std::vector<WorldSave::BoatRecord> boats;
  REQUIRE(WorldSave::loadBoats(dir.path.string(), boats));
  REQUIRE(boats.size() == 1);
  CHECK(boats[0].seats[0].rider == WorldSave::BoatRecord::Rider::Host);
  CHECK(boats[0].seats[1].name == b_name);
}

TEST_CASE_FIXTURE(SavedAtSea, "a guest who wasn't back yet keeps their seat through another save") {
  {
    Reloaded world(dir.path.string());
    LocalConnection host;
    joinAs(world.game, host, "", Game::Role::Host);
    REQUIRE(world.game.save());
  }
  Reloaded world(dir.path.string());
  LocalConnection host, guest;
  joinAs(world.game, host, "", Game::Role::Host);
  auto riding = find<Riding>(joinAs(world.game, guest, b_name, Game::Role::Guest));
  REQUIRE(riding);
  CHECK(riding->seat == 1);
}

TEST_CASE_FIXTURE(SavedAtSea, "a seat taken in the meantime leaves you standing where you were saved") {
  Reloaded world(dir.path.string());
  LocalConnection host, other, guest;
  joinAs(world.game, host, "", Game::Role::Host);
  // Someone else climbs into the passenger seat first.
  auto other_evs = joinAs(world.game, other, "Zed", Game::Role::Guest);
  PlayerState &zed = *world.game.findPlayer(find<Joined>(other_evs)->id);
  MovePlayer near;
  near.position = boat_at + glm::vec3(-1.0f, 0.2f, 0.0f);
  near.respawn = zed.respawn;
  other.send(near);
  other.send(BoardBoat{world.boat()});
  world.game.pump();
  REQUIRE(zed.riding);
  REQUIRE(zed.riding->seat == 1);

  auto evs = joinAs(world.game, guest, b_name, Game::Role::Guest);
  CHECK_FALSE(find<Riding>(evs));
  CHECK_FALSE(world.game.findPlayer(find<Joined>(evs)->id)->riding);
}
