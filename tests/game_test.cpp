// Game is exercised through a LocalConnection exactly as Engine drives it,
// without a window: World::init() (shaders) is never called.
#include "doctest/doctest.h"
#include "core/settings.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include <cmath>

namespace {

// Joining makes the Game stream chunks around the player. Radius 0 streams
// only the chunk each player stands in, so tests don't generate a full render
// distance of terrain and know exactly which chunks can appear.
struct SmallGame {
  ScopedRenderDistance rd{0};
  Game game;
};

std::vector<Event> pollAll(LocalConnection &conn) {
  std::vector<Event> out;
  conn.poll(out);
  return out;
}

// Sends Join, pumps, and returns the Joined reply.
Joined join(Game &game, LocalConnection &conn) {
  game.addClient(conn);
  conn.send(Join{});
  game.pump();
  auto events = pollAll(conn);
  REQUIRE(events.size() == 1);
  REQUIRE(std::holds_alternative<Joined>(events[0]));
  return std::get<Joined>(events[0]);
}

} // namespace

TEST_CASE_FIXTURE(SmallGame, "advance runs whole fixed ticks and carries the remainder") {
  CHECK(game.advance(0.125f) == 2);  // 100 ms used, 25 ms carried
  CHECK(game.advance(0.025f) == 1);  // carried 25 + 25 = one more tick
  CHECK(game.advance(0.049f) == 0);
  CHECK(game.advance(0.0f) == 0);
}

TEST_CASE_FIXTURE(SmallGame, "the clock advances only on ticks") {
  float saved_scale = g_settings.time_scale;
  g_settings.time_scale = 72.0f;

  float t0 = game.clock().time_of_day;
  game.advance(0.04f); // under one tick
  CHECK(game.clock().time_of_day == t0);
  game.advance(0.02f); // crosses the tick boundary
  CHECK(game.clock().time_of_day ==
        doctest::Approx(t0 + Game::kTickSeconds * 72.0f));

  g_settings.time_scale = saved_scale;
}

TEST_CASE_FIXTURE(SmallGame, "joining a fresh world spawns on solid ground") {
  LocalConnection conn;
  Joined j = join(game, conn);

  CHECK(j.id != 0);
  REQUIRE(game.findPlayer(j.id) != nullptr);
  CHECK(game.findPlayer(j.id)->position == j.position);

  int cx = static_cast<int>(std::floor(j.position.x / kChunkWidth));
  int cz = static_cast<int>(std::floor(j.position.z / kChunkDepth));
  game.world().loadChunkSync(cx, cz);
  CHECK(game.world().isSolidBlock(static_cast<int>(std::floor(j.position.x)),
                                  static_cast<int>(j.position.y) - 1,
                                  static_cast<int>(std::floor(j.position.z))));
}

TEST_CASE_FIXTURE(SmallGame, "the first join restores the saved player") {
  WorldSave::LevelData level;
  level.time_of_day = 1000.0f;
  WorldSave::PlayerData pd;
  pd.position = {12.5f, 90.0f, -7.5f};
  pd.yaw = 30.0f;
  pd.pitch = -10.0f;
  pd.selected_slot = 6;
  pd.fly_mode = 1;

  game.restore(level, {}, pd);
  CHECK(game.clock().time_of_day == 1000.0f);

  LocalConnection conn;
  Joined j = join(game, conn);
  CHECK(j.position == pd.position);
  CHECK(j.yaw == pd.yaw);
  CHECK(j.pitch == pd.pitch);
  CHECK(j.slot == 6);
  CHECK(j.fly);

  // A second client gets a fresh spawn, not the same saved record.
  LocalConnection conn2;
  Joined j2 = join(game, conn2);
  CHECK(j2.id != j.id);
  CHECK(j2.position != pd.position);
}

TEST_CASE_FIXTURE(SmallGame, "joining twice on one connection is ignored") {
  LocalConnection conn;
  join(game, conn);
  conn.send(Join{});
  game.pump();
  CHECK(pollAll(conn).empty());
}

TEST_CASE_FIXTURE(SmallGame, "SetTimeOfDay sets and wraps the clock") {
  LocalConnection conn;
  join(game, conn);

  conn.send(SetTimeOfDay{6 * 3600.0f});
  game.pump();
  CHECK(game.clock().time_of_day == 6 * 3600.0f);

  conn.send(SetTimeOfDay{24 * 3600.0f});
  game.pump();
  CHECK(game.clock().time_of_day == 0.0f);
}

TEST_CASE_FIXTURE(SmallGame, "MovePlayer updates the player's state") {
  LocalConnection conn;
  Joined j = join(game, conn);

  conn.send(MovePlayer{{40.5f, 70.0f, -8.5f}, 15.0f, -20.0f, true, true});
  game.pump();

  const PlayerState *ps = game.findPlayer(j.id);
  REQUIRE(ps);
  CHECK(ps->position == glm::vec3(40.5f, 70.0f, -8.5f));
  CHECK(ps->yaw == 15.0f);
  CHECK(ps->pitch == -20.0f);
  CHECK(ps->fly);
  CHECK(ps->crouch);
  CHECK(ps->height() == kPlayerCrouchHeight);
}

TEST_CASE_FIXTURE(SmallGame, "chunks stream around each player's position") {
  LocalConnection conn;
  join(game, conn);

  const glm::vec3 far{200.5f * kChunkWidth, 80.0f, 0.5f};
  conn.send(MovePlayer{far, 0.0f, 0.0f, false, false});
  game.pump();

  CHECK(waitForChunk(game.world(), 200, 0));
}

TEST_CASE_FIXTURE(SmallGame, "SelectSlot stores valid slots and rejects others") {
  LocalConnection conn;
  Joined j = join(game, conn);

  conn.send(SelectSlot{3});
  game.pump();
  CHECK(game.findPlayer(j.id)->selected_slot == 3);
  CHECK(pollAll(conn).empty());

  conn.send(SelectSlot{42});
  conn.send(SelectSlot{-1});
  game.pump();
  CHECK(game.findPlayer(j.id)->selected_slot == 3);
  auto events = pollAll(conn);
  REQUIRE(events.size() == 2);
  CHECK(std::holds_alternative<CommandRejected>(events[0]));
}

TEST_CASE_FIXTURE(SmallGame, "commands before Join are ignored") {
  LocalConnection conn;
  game.addClient(conn);
  conn.send(MovePlayer{{1.0f, 2.0f, 3.0f}, 0.0f, 0.0f, false, false});
  conn.send(SelectSlot{2});
  game.pump();
  CHECK(pollAll(conn).empty());
}

// ─── Block edits ───────────────────────────────────────────────────────

namespace {

// A joined player standing on the terrain of chunk (0,0), next to column
// (5,5). Two connections so broadcasts can be checked.
struct EditGame : SmallGame {
  LocalConnection conn, other;
  PlayerId id = 0;
  int ground = 0; // top solid block y in column (5,5)

  EditGame() {
    game.world().loadChunkSync(0, 0);
    for (int y = kChunkHeight - 1; y >= 0; --y) {
      BlockType b = game.world().getBlockAt(glm::vec3(5.5f, y + 0.5f, 5.5f));
      if (b != BlockType::AIR && b != BlockType::WATER) { ground = y; break; }
    }
    REQUIRE(ground > 0);
    id = join(game, conn).id;
    // A second player far away, only to observe broadcasts.
    game.addClient(other);
    other.send(Join{});
    game.pump();
    pollAll(other);
    moveTo({5.5f, ground + 1.0f, 8.5f});
  }

  void moveTo(glm::vec3 pos, bool crouch = false) {
    conn.send(MovePlayer{pos, 0.0f, 0.0f, false, crouch});
    game.pump();
  }

  BlockType at(glm::ivec3 p) const {
    return game.world().getBlockAt(glm::vec3(p) + 0.5f);
  }

  // Sends one command, pumps, and returns this client's events.
  std::vector<Event> run(Command cmd) {
    conn.send(std::move(cmd));
    game.pump();
    return pollAll(conn);
  }

  static bool rejected(const std::vector<Event> &evs) {
    return evs.size() == 1 && std::holds_alternative<CommandRejected>(evs[0]);
  }
};

} // namespace

TEST_CASE_FIXTURE(EditGame, "a valid break removes the block and broadcasts once") {
  glm::ivec3 target{5, ground, 5};
  auto evs = run(BreakBlock{target});

  CHECK(at(target) == BlockType::AIR);
  REQUIRE(evs.size() == 1);
  REQUIRE(std::holds_alternative<BlockChanged>(evs[0]));
  CHECK(std::get<BlockChanged>(evs[0]).pos == target);
  CHECK(std::get<BlockChanged>(evs[0]).type == BlockType::AIR);
  CHECK(pollAll(other).size() == 1);
}

TEST_CASE_FIXTURE(EditGame, "a valid place sets the block and broadcasts once") {
  glm::ivec3 target{5, ground + 1, 5};
  auto evs = run(PlaceBlock{target, BlockType::COBBLESTONE});

  CHECK(at(target) == BlockType::COBBLESTONE);
  REQUIRE(evs.size() == 1);
  CHECK(std::get<BlockChanged>(evs[0]).type == BlockType::COBBLESTONE);
  CHECK(pollAll(other).size() == 1);
}

TEST_CASE_FIXTURE(EditGame, "bedrock, water and air cannot be broken") {
  // Placed directly: generated bedrock isn't guaranteed at any one cell.
  glm::ivec3 bedrock{5, ground + 1, 5};
  game.world().setBlockAt(bedrock, BlockType::BEDROCK);
  CHECK(rejected(run(BreakBlock{bedrock})));
  CHECK(at(bedrock) == BlockType::BEDROCK);

  glm::ivec3 water{5, ground + 2, 5};
  game.world().setBlockAt(water, BlockType::WATER);
  CHECK(rejected(run(BreakBlock{water})));
  CHECK(at(water) == BlockType::WATER);

  CHECK(rejected(run(BreakBlock{{5, ground + 3, 5}})));
  CHECK(pollAll(other).empty());
}

TEST_CASE_FIXTURE(EditGame, "edits out of reach are rejected") {
  glm::ivec3 far{5, ground, 5};
  moveTo({5.5f, ground + 1.0f, 5.5f + 8.0f});
  CHECK(rejected(run(BreakBlock{far})));
  CHECK(rejected(run(PlaceBlock{far + glm::ivec3(0, 1, 0), BlockType::STONE})));
  CHECK(at(far) != BlockType::AIR);
}

TEST_CASE_FIXTURE(EditGame, "placing outside the world height is rejected") {
  moveTo({5.5f, kChunkHeight - 1.0f, 8.5f});
  CHECK(rejected(run(PlaceBlock{{5, kChunkHeight, 5}, BlockType::STONE})));
  moveTo({5.5f, 0.0f, 8.5f});
  CHECK(rejected(run(PlaceBlock{{5, -1, 5}, BlockType::STONE})));
}

TEST_CASE_FIXTURE(EditGame, "placing AIR or into an occupied cell is rejected") {
  CHECK(rejected(run(PlaceBlock{{5, ground + 1, 5}, BlockType::AIR})));
  CHECK(rejected(run(PlaceBlock{{5, ground, 5}, BlockType::STONE})));
}

TEST_CASE_FIXTURE(EditGame, "placing into water replaces it") {
  glm::ivec3 water{5, ground + 1, 5};
  game.world().setBlockAt(water, BlockType::WATER);
  auto evs = run(PlaceBlock{water, BlockType::SAND});
  CHECK(at(water) == BlockType::SAND);
  CHECK(evs.size() == 1);
}

TEST_CASE_FIXTURE(EditGame, "placing inside your own box is rejected") {
  const glm::vec3 feet{5.5f, ground + 1.0f, 5.5f};
  moveTo(feet);
  CHECK(rejected(run(PlaceBlock{{5, ground + 1, 5}, BlockType::STONE})));
  CHECK(rejected(run(PlaceBlock{{5, ground + 2, 5}, BlockType::STONE})));
  CHECK(run(PlaceBlock{{5, ground + 3, 5}, BlockType::STONE}).size() == 1);
}

TEST_CASE_FIXTURE(EditGame, "crouching lowers the box used for placement") {
  // Feet 0.4 into the cell: standing (1.8) reaches into ground+3,
  // crouched (1.5) stops below it.
  glm::ivec3 cell{5, ground + 3, 5};
  moveTo({5.5f, ground + 1.4f, 5.5f});
  CHECK(rejected(run(PlaceBlock{cell, BlockType::STONE})));

  moveTo({5.5f, ground + 1.4f, 5.5f}, /*crouch=*/true);
  CHECK(run(PlaceBlock{cell, BlockType::STONE}).size() == 1);
  CHECK(at(cell) == BlockType::STONE);
}

TEST_CASE_FIXTURE(EditGame, "placing inside another player's box is rejected") {
  // Move the observer next to the target cell.
  other.send(MovePlayer{{5.5f, ground + 1.0f, 6.5f}, 0, 0, false, false});
  game.pump();
  CHECK(rejected(run(PlaceBlock{{5, ground + 1, 6}, BlockType::STONE})));
}

TEST_CASE_FIXTURE(EditGame, "placing into an unloaded chunk is rejected") {
  // Both players stay in chunk (0,0), so chunk (-1,0) is never streamed.
  other.send(MovePlayer{{8.5f, ground + 1.0f, 8.5f}, 0, 0, false, false});
  moveTo({0.5f, ground + 1.0f, 5.5f});
  REQUIRE(game.world().getChunk(-1, 0) == nullptr);
  CHECK(rejected(run(PlaceBlock{{-1, ground + 1, 5}, BlockType::STONE})));
}

TEST_CASE_FIXTURE(EditGame, "glowstone placed through the Game lights and unlights") {
  glm::ivec3 lamp{5, ground + 2, 5};
  REQUIRE(run(PlaceBlock{lamp, BlockType::GLOWSTONE}).size() == 1);
  CHECK(game.world().getBlockLightAt(lamp + glm::ivec3(1, 0, 0)) > 0);

  REQUIRE(run(BreakBlock{lamp}).size() == 1);
  CHECK(game.world().getBlockLightAt(lamp + glm::ivec3(1, 0, 0)) == 0);
}
