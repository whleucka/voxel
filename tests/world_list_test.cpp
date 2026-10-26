#include "doctest/doctest.h"
#include "test_util.hpp"
#include "world/world_list.hpp"
#include "world/world_save.hpp"
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using namespace std::chrono;

TEST_CASE("world names") {
  CHECK(validWorldName("world"));
  CHECK(validWorldName("Bea's castle") == false); // apostrophe
  CHECK(validWorldName("Big Castle 2"));
  CHECK(validWorldName("snow_fort-1"));
  CHECK(validWorldName(std::string(32, 'a')));
  CHECK_FALSE(validWorldName(std::string(33, 'a')));
  CHECK_FALSE(validWorldName(""));
  CHECK_FALSE(validWorldName(" padded"));
  CHECK_FALSE(validWorldName("padded "));
  CHECK_FALSE(validWorldName("."));
  CHECK_FALSE(validWorldName(".."));
  CHECK_FALSE(validWorldName("a/b"));
  CHECK_FALSE(validWorldName("..\\x"));
}

TEST_CASE("a created world lists, and loads with its seed") {
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "castle", "12345"));
  WorldSave::LevelData level;
  REQUIRE(WorldSave::loadLevel((saves.path / "castle").string(), level));
  CHECK(level.seed == 12345u);
  CHECK(level.noise_offset == glm::vec2(2345.0f, 2.0f));
  CHECK_FALSE(fs::exists(saves.path / "castle" / "player.dat")); // spawn is found fresh

  auto worlds = listWorlds(saves.path);
  REQUIRE(worlds.size() == 1);
  CHECK(worlds[0].name == "castle");
  CHECK(worlds[0].seed == 12345u);
  CHECK(worlds[0].size_bytes > 0);
}

TEST_CASE("seed text works like voxel.properties") {
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "a", "hello"));
  REQUIRE_FALSE(createWorld(saves.path, "b", "hello"));
  REQUIRE_FALSE(createWorld(saves.path, "c", ""));
  auto worlds = listWorlds(saves.path);
  auto seedOf = [&](const std::string &n) {
    for (auto &w : worlds)
      if (w.name == n) return w.seed;
    FAIL("missing world");
    return 0u;
  };
  CHECK(seedOf("a") == seedOf("b"));
}

TEST_CASE("creating refuses bad and taken names") {
  TempDir saves;
  CHECK(createWorld(saves.path, "../escape", "1"));
  CHECK(createWorld(saves.path, "", "1"));
  REQUIRE_FALSE(createWorld(saves.path, "castle", "1"));
  auto taken = createWorld(saves.path, "castle", "2");
  REQUIRE(taken);
  CHECK(*taken == "There's already a world called 'castle'");
  CHECK_FALSE(fs::exists(saves.path.parent_path() / "escape"));
}

TEST_CASE("worlds list most recently played first, and skip non-worlds") {
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "old", "1"));
  REQUIRE_FALSE(createWorld(saves.path, "new", "2"));
  fs::last_write_time(saves.path / "old" / "level.dat",
                      fs::file_time_type::clock::now() - hours(48));
  fs::create_directories(saves.path / "empty folder");
  fs::create_directories(saves.path / "not.a.world");
  std::ofstream(saves.path / "stray.txt") << "x";

  auto worlds = listWorlds(saves.path);
  REQUIRE(worlds.size() == 2);
  CHECK(worlds[0].name == "new");
  CHECK(worlds[1].name == "old");
}

TEST_CASE("a missing saves folder lists nothing") {
  CHECK(listWorlds("/nonexistent/voxel/saves").empty());
}

TEST_CASE("renaming moves the world and keeps its contents") {
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "castle", "7"));
  std::ofstream(saves.path / "castle" / "edits.dat") << "keep me";
  REQUIRE_FALSE(renameWorld(saves.path, "castle", "Big Castle"));
  CHECK_FALSE(fs::exists(saves.path / "castle"));
  CHECK(fs::exists(saves.path / "Big Castle" / "edits.dat"));
  CHECK(listWorlds(saves.path)[0].seed == 7u);
}

TEST_CASE("renaming refuses bad, taken and missing names") {
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "a", "1"));
  REQUIRE_FALSE(createWorld(saves.path, "b", "2"));
  CHECK(renameWorld(saves.path, "a", "b"));
  CHECK(renameWorld(saves.path, "a", "../a"));
  CHECK(renameWorld(saves.path, "nope", "c"));
  CHECK(renameWorld(saves.path, "../a", "c"));
  CHECK(listWorlds(saves.path).size() == 2);
}

TEST_CASE("deleting removes exactly that world") {
  TempDir saves;
  REQUIRE_FALSE(createWorld(saves.path, "a", "1"));
  REQUIRE_FALSE(createWorld(saves.path, "b", "2"));
  REQUIRE_FALSE(deleteWorld(saves.path, "a"));
  CHECK_FALSE(fs::exists(saves.path / "a"));
  CHECK(fs::exists(saves.path / "b" / "level.dat"));
}

TEST_CASE("deleting refuses escapes, missing worlds and non-worlds") {
  TempDir outer;
  fs::path saves = outer.path / "saves";
  fs::create_directories(saves / "keepme"); // a folder, but not a world
  std::ofstream(outer.path / "precious.txt") << "x";
  CHECK(deleteWorld(saves, ".."));
  CHECK(deleteWorld(saves, "../precious.txt"));
  CHECK(deleteWorld(saves, "missing"));
  CHECK(deleteWorld(saves, "keepme"));
  CHECK(deleteWorld(saves, ""));
  CHECK(fs::exists(outer.path / "precious.txt"));
  CHECK(fs::exists(saves / "keepme"));
}

TEST_CASE("last played reads like a person would say it") {
  // Noon today, local time, so "today" and "yesterday" are unambiguous.
  std::time_t t = std::time(nullptr);
  std::tm noon{};
  localtime_r(&t, &noon);
  noon.tm_hour = 12, noon.tm_min = 0, noon.tm_sec = 0, noon.tm_isdst = -1;
  auto now = system_clock::from_time_t(std::mktime(&noon));

  CHECK(lastPlayedText(now - seconds(20), now) == "just now");
  CHECK(lastPlayedText(now - minutes(1), now) == "1 minute ago");
  CHECK(lastPlayedText(now - minutes(5), now) == "5 minutes ago");
  CHECK(lastPlayedText(now - hours(2), now) == "today 10:00");
  CHECK(lastPlayedText(now - hours(20), now) == "yesterday 16:00");
  CHECK(lastPlayedText(now - hours(24 * 3), now) == "3 days ago");
  auto old = lastPlayedText(now - hours(24 * 60), now);
  CHECK(old.size() == 10); // YYYY-MM-DD
  CHECK(old[4] == '-');
}
