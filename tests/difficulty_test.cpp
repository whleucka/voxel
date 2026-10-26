#include "doctest/doctest.h"
#include "core/config.hpp"
#include "core/difficulty.hpp"
#include "core/settings.hpp"
#include "test_util.hpp"
#include <fstream>

namespace {

// loadServerProperties writes into g_settings; put it back afterwards.
struct ScopedSettings {
  Settings saved = g_settings;
  ~ScopedSettings() { g_settings = saved; }
};

} // namespace

TEST_CASE("difficulty names parse and print") {
  for (Difficulty d : {Difficulty::Peaceful, Difficulty::Easy, Difficulty::Normal})
    CHECK(parseDifficulty(difficultyName(d)) == d);
  CHECK_FALSE(parseDifficulty("hard"));
  CHECK_FALSE(parseDifficulty(""));
}

TEST_CASE("the default difficulty is easy") {
  CHECK(Settings{}.difficulty == Difficulty::Easy);

  ScopedSettings keep;
  TempDir dir;
  std::string path = (dir.path / "voxel.properties").string();
  g_settings.difficulty = Difficulty::Normal;
  loadServerProperties(path); // no file yet: writes and loads the defaults
  CHECK(g_settings.difficulty == Difficulty::Easy);
}

TEST_CASE("difficulty round-trips through voxel.properties") {
  ScopedSettings keep;
  TempDir dir;
  std::string path = (dir.path / "voxel.properties").string();
  for (Difficulty d : {Difficulty::Peaceful, Difficulty::Normal, Difficulty::Easy}) {
    g_settings.difficulty = d;
    REQUIRE(saveServerProperties(path));
    g_settings.difficulty = d == Difficulty::Easy ? Difficulty::Normal : Difficulty::Easy;
    loadServerProperties(path);
    CHECK(g_settings.difficulty == d);
  }
}

TEST_CASE("an unknown difficulty keeps the current one") {
  ScopedSettings keep;
  TempDir dir;
  std::string path = (dir.path / "voxel.properties").string();
  std::ofstream(path) << "difficulty=nightmare\n";
  g_settings.difficulty = Difficulty::Normal;
  loadServerProperties(path);
  CHECK(g_settings.difficulty == Difficulty::Normal);
}

TEST_CASE("easy halves caps and damage, at least 1, and slows enemies") {
  CHECK(scaledCap(4, Difficulty::Easy) == 2);
  CHECK(scaledCap(3, Difficulty::Easy) == 1);
  CHECK(scaledCap(1, Difficulty::Easy) == 1);
  CHECK(scaledDamage(2, Difficulty::Easy) == 1);
  CHECK(scaledDamage(1, Difficulty::Easy) == 1);
  CHECK(speedScale(Difficulty::Easy) == doctest::Approx(0.8f));

  CHECK(scaledCap(4, Difficulty::Normal) == 4);
  CHECK(scaledDamage(2, Difficulty::Normal) == 2);
  CHECK(speedScale(Difficulty::Normal) == 1.0f);
}

TEST_CASE("player name and server port round-trip through voxel.properties") {
  ScopedSettings keep;
  TempDir dir;
  std::string path = (dir.path / "voxel.properties").string();
  CHECK(Settings{}.player_name == "Player");
  CHECK(Settings{}.server_port == 47800);
  g_settings.player_name = "Bea";
  g_settings.server_port = 50123;
  REQUIRE(saveServerProperties(path));
  g_settings.player_name = "x";
  g_settings.server_port = 1;
  loadServerProperties(path);
  CHECK(g_settings.player_name == "Bea");
  CHECK(g_settings.server_port == 50123);
}
