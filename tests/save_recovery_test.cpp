// A damaged save must never cost more than the damage: whatever is left of a
// file that won't load is kept beside it, rather than saved over.
#include "doctest/doctest.h"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include <fstream>

namespace {

namespace fs = std::filesystem;

std::string slurp(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}

// A saved world with 1000 edits across ten chunks.
void writeWorld(const fs::path &dir) {
  WorldSave::LevelData level;
  level.seed = 4242;
  WorldEdits edits;
  for (int c = 0; c < 10; ++c)
    for (uint32_t i = 0; i < 100; ++i) edits[{c, 0}][i] = static_cast<uint8_t>(BlockType::STONE);
  REQUIRE(WorldSave::save(dir.string(), level, {}, edits));
}

void cutInHalf(const fs::path &file) { fs::resize_file(file, fs::file_size(file) / 2); }

} // namespace

TEST_CASE("a cut-short edits.dat is set aside whole, and the next save doesn't replace it") {
  ScopedSeed seed;
  ScopedRenderDistance rd{0};
  TempDir dir;
  writeWorld(dir.path);
  cutInHalf(dir.path / "edits.dat");
  const std::string damaged = slurp(dir.path / "edits.dat");

  Game game;
  REQUIRE(game.load(dir.path.string()));
  REQUIRE(game.save());
  CHECK(slurp(dir.path / "edits.dat.unreadable") == damaged);
}

TEST_CASE("an unreadable chests.dat isn't saved over with empty chests") {
  ScopedSeed seed;
  ScopedRenderDistance rd{0};
  TempDir dir;
  writeWorld(dir.path);
  WorldSave::ChestRecord chest;
  chest.pos = {1, 2, 3};
  chest.chest[0] = {ItemId::DIAMOND, 5};
  REQUIRE(WorldSave::saveChests(dir.path.string(), std::vector{chest}));
  cutInHalf(dir.path / "chests.dat");
  const std::string damaged = slurp(dir.path / "chests.dat");

  Game game;
  REQUIRE(game.load(dir.path.string()));
  REQUIRE(game.save());
  CHECK(slurp(dir.path / "chests.dat.unreadable") == damaged);
}

TEST_CASE("an unreadable level.dat sets the whole world aside instead of overwriting it") {
  ScopedSeed seed;
  ScopedRenderDistance rd{0};
  TempDir dir;
  writeWorld(dir.path);
  const std::string edits = slurp(dir.path / "edits.dat");
  cutInHalf(dir.path / "level.dat");

  Game game;
  CHECK_FALSE(game.load(dir.path.string())); // starts fresh
  REQUIRE(game.save());
  CHECK(fs::exists(dir.path / "level.dat.unreadable"));
  CHECK(slurp(dir.path / "edits.dat.unreadable") == edits);
}

TEST_CASE("a saved hotbar slot out of range is clamped, not trusted") {
  ScopedRenderDistance rd{0};
  Game game;
  game.spawner() = Spawner{};
  WorldSave::PlayerData pd;
  pd.position = {8.5f, 200.0f, 8.5f};
  pd.selected_slot = 99;
  game.restore({}, {}, pd);

  LocalConnection conn;
  game.addClient(conn);
  conn.send(Join{});
  game.pump();
  const PlayerState *me = nullptr;
  for (PlayerId id = 0; id < 8 && !me; ++id) me = game.findPlayer(id);
  REQUIRE(me);
  CHECK(me->selected_slot >= 0);
  CHECK(me->selected_slot < kHotbarSlots);

  // Dropping reads the held slot: it must not throw.
  conn.send(DropItem{false});
  CHECK_NOTHROW(game.pump());
}

// ─── Autosave ──────────────────────────────────────────────────────────
// A crash costs at most the last few minutes.

TEST_CASE("a game bound to a save directory saves itself every few minutes") {
  ScopedSeed seed;
  ScopedRenderDistance rd{0};
  TempDir dir;
  Game game;
  game.spawner() = Spawner{};
  CHECK_FALSE(game.load(dir.path.string())); // a new world: binds the directory
  LocalConnection conn;
  game.addClient(conn, Game::Role::Host);
  conn.send(Join{});
  game.pump();

  const fs::path level = dir.path / "level.dat";
  for (int i = 0; i < Game::kAutosaveTicks - 1; ++i) game.tick();
  CHECK_FALSE(fs::exists(level));
  game.tick();
  REQUIRE(fs::exists(level));
  WorldSave::LevelData saved;
  REQUIRE(WorldSave::loadLevel(dir.path.string(), saved));
  CHECK(saved.time_of_day == doctest::Approx(game.clock().time_of_day));

  // And again, with what has changed since.
  for (int i = 0; i < Game::kAutosaveTicks; ++i) game.tick();
  REQUIRE(WorldSave::loadLevel(dir.path.string(), saved));
  CHECK(saved.time_of_day == doctest::Approx(game.clock().time_of_day));
}
