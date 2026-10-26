// Game is exercised through a LocalConnection exactly as Engine drives it,
// without a window: World::init() (shaders) is never called.
#include "doctest/doctest.h"
#include "core/settings.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include <cmath>

namespace {

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

TEST_CASE("advance runs whole fixed ticks and carries the remainder") {
  Game game;
  CHECK(game.advance(0.125f) == 2);  // 100 ms used, 25 ms carried
  CHECK(game.advance(0.025f) == 1);  // carried 25 + 25 = one more tick
  CHECK(game.advance(0.049f) == 0);
  CHECK(game.advance(0.0f) == 0);
}

TEST_CASE("the clock advances only on ticks") {
  float saved_scale = g_settings.time_scale;
  g_settings.time_scale = 72.0f;

  Game game;
  float t0 = game.clock().time_of_day;
  game.advance(0.04f); // under one tick
  CHECK(game.clock().time_of_day == t0);
  game.advance(0.02f); // crosses the tick boundary
  CHECK(game.clock().time_of_day ==
        doctest::Approx(t0 + Game::kTickSeconds * 72.0f));

  g_settings.time_scale = saved_scale;
}

TEST_CASE("joining a fresh world spawns on solid ground") {
  Game game;
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

TEST_CASE("the first join restores the saved player") {
  WorldSave::LevelData level;
  level.time_of_day = 1000.0f;
  WorldSave::PlayerData pd;
  pd.position = {12.5f, 90.0f, -7.5f};
  pd.yaw = 30.0f;
  pd.pitch = -10.0f;
  pd.selected_slot = 6;
  pd.fly_mode = 1;

  Game game;
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

TEST_CASE("joining twice on one connection is ignored") {
  Game game;
  LocalConnection conn;
  join(game, conn);
  conn.send(Join{});
  game.pump();
  CHECK(pollAll(conn).empty());
}

TEST_CASE("SetTimeOfDay sets and wraps the clock") {
  Game game;
  LocalConnection conn;
  join(game, conn);

  conn.send(SetTimeOfDay{6 * 3600.0f});
  game.pump();
  CHECK(game.clock().time_of_day == 6 * 3600.0f);

  conn.send(SetTimeOfDay{24 * 3600.0f});
  game.pump();
  CHECK(game.clock().time_of_day == 0.0f);
}
