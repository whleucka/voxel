// Game is exercised through a LocalConnection exactly as Engine drives it,
// without a window: World::init() (shaders) is never called.
#include "doctest/doctest.h"
#include "core/settings.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include <cmath>

namespace {

// Joining makes the Game stream chunks around the player; keep the radius
// tiny so tests don't generate a full render distance of terrain.
struct SmallGame {
  ScopedRenderDistance rd{1};
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
