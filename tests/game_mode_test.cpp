#include "doctest/doctest.h"
#include "core/game_mode.hpp"
#include "sim/game.hpp"
#include "test_util.hpp"
#include "world/world_list.hpp"
#include "world/world_save.hpp"
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("game mode names parse and print") {
  for (GameMode m : {GameMode::Survival, GameMode::Creative})
    CHECK(parseGameMode(gameModeName(m)) == m);
  CHECK_FALSE(parseGameMode("hardcore"));
}

TEST_CASE("survival and creative rules") {
  ModeRules s = rulesFor(GameMode::Survival);
  CHECK_FALSE(s.fly);
  CHECK(s.consume_blocks);
  CHECK(s.block_drops);
  CHECK(s.damage);
  CHECK(s.drain);
  CHECK(s.enemies_target);
  CHECK_FALSE(s.palette);

  ModeRules c = rulesFor(GameMode::Creative);
  CHECK(c.fly);
  CHECK_FALSE(c.consume_blocks);
  CHECK_FALSE(c.block_drops);
  CHECK_FALSE(c.damage);
  CHECK_FALSE(c.drain);
  CHECK_FALSE(c.enemies_target);
  CHECK(c.palette);
}

TEST_CASE("level.dat round-trips the mode") {
  TempDir dir;
  WorldSave::LevelData level;
  level.seed = 99;
  level.mode = GameMode::Creative;
  REQUIRE(WorldSave::saveLevel(dir.path.string(), level));
  WorldSave::LevelData back;
  REQUIRE(WorldSave::loadLevel(dir.path.string(), back));
  CHECK(back.seed == 99u);
  CHECK(back.mode == GameMode::Creative);
}

TEST_CASE("a level.dat from before modes loads as survival") {
  TempDir dir;
  WorldSave::LevelData level;
  level.seed = 5;
  level.time_of_day = 100.0f;
  level.mode = GameMode::Creative;
  REQUIRE(WorldSave::saveLevel(dir.path.string(), level));
  fs::path file = dir.path / "level.dat";
  fs::resize_file(file, fs::file_size(file) - 2); // drop the mode and worldgen bytes

  WorldSave::LevelData back;
  REQUIRE(WorldSave::loadLevel(dir.path.string(), back));
  CHECK(back.seed == 5u);
  CHECK(back.time_of_day == 100.0f);
  CHECK(back.mode == GameMode::Survival);
}

TEST_CASE("the world list creates, changes and lists the mode") {
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "build", "1", GameMode::Creative));
  REQUIRE_FALSE(createWorld(saves.path, "plain", "2"));
  auto modeOf = [&](const std::string &n) {
    for (const WorldInfo &w : listWorlds(saves.path))
      if (w.name == n) return w.mode;
    FAIL("missing world");
    return GameMode::Survival;
  };
  CHECK(modeOf("build") == GameMode::Creative);
  CHECK(modeOf("plain") == GameMode::Survival);

  std::ofstream(saves.path / "plain" / "edits.dat") << "keep me";
  auto before = fs::last_write_time(saves.path / "plain" / "level.dat");
  REQUIRE_FALSE(setWorldMode(saves.path, "plain", GameMode::Creative));
  CHECK(modeOf("plain") == GameMode::Creative);
  CHECK(fs::last_write_time(saves.path / "plain" / "level.dat") == before);
  CHECK(fs::exists(saves.path / "plain" / "edits.dat"));

  WorldSave::LevelData level;
  REQUIRE(WorldSave::loadLevel((saves.path / "plain").string(), level));
  CHECK(level.seed == 2u);
}

TEST_CASE("changing the mode refuses missing worlds and escapes") {
  TempDir saves;
  CHECK(setWorldMode(saves.path, "nope", GameMode::Creative));
  CHECK(setWorldMode(saves.path, "..", GameMode::Creative));
  CHECK(setWorldMode(saves.path, "a/b", GameMode::Creative));
}

TEST_CASE("the game keeps a world's mode when it saves") {
  ScopedRenderDistance rd{0};
  Settings saved = g_settings;
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "build", "3", GameMode::Creative));
  std::string dir = (saves.path / "build").string();
  {
    Game game;
    REQUIRE(game.load(dir));
    CHECK(game.mode() == GameMode::Creative);
    CHECK(game.rules().fly);
    REQUIRE(game.save());
  }
  WorldSave::LevelData level;
  REQUIRE(WorldSave::loadLevel(dir, level));
  CHECK(level.mode == GameMode::Creative);
  g_settings = saved;
}

// ─── Worldgen version ──────────────────────────────────────────────────

TEST_CASE("level.dat round-trips the worldgen version") {
  TempDir dir;
  WorldSave::LevelData level;
  level.worldgen = worldgen::kOriginal;
  REQUIRE(WorldSave::saveLevel(dir.path.string(), level));
  WorldSave::LevelData back;
  REQUIRE(WorldSave::loadLevel(dir.path.string(), back));
  CHECK(back.worldgen == worldgen::kOriginal);
}

TEST_CASE("a level.dat from before caves loads as the original generator") {
  TempDir dir;
  WorldSave::LevelData level;
  level.mode = GameMode::Creative;
  REQUIRE(WorldSave::saveLevel(dir.path.string(), level));
  fs::path file = dir.path / "level.dat";
  fs::resize_file(file, fs::file_size(file) - 1); // drop the worldgen byte

  WorldSave::LevelData back;
  REQUIRE(WorldSave::loadLevel(dir.path.string(), back));
  CHECK(back.mode == GameMode::Creative);
  CHECK(back.worldgen == worldgen::kOriginal);
}

TEST_CASE("new worlds get caves; an old world keeps its generator through a save") {
  ScopedRenderDistance rd{0};
  Settings saved = g_settings;
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "fresh", "3", GameMode::Survival));
  WorldSave::LevelData level;
  REQUIRE(WorldSave::loadLevel((saves.path / "fresh").string(), level));
  CHECK(level.worldgen == worldgen::kCaves);

  std::string dir = (saves.path / "old").string();
  level.worldgen = worldgen::kOriginal;
  REQUIRE(WorldSave::saveLevel(dir, level));
  {
    Game game;
    REQUIRE(game.load(dir));
    CHECK(g_settings.worldgen == worldgen::kOriginal);
    REQUIRE(game.save());
  }
  REQUIRE(WorldSave::loadLevel(dir, level));
  CHECK(level.worldgen == worldgen::kOriginal);
  g_settings = saved;
}
