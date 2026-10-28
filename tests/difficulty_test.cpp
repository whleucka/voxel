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
  for (Difficulty d :
       {Difficulty::Peaceful, Difficulty::Easy, Difficulty::Normal, Difficulty::Hard})
    CHECK(parseDifficulty(difficultyName(d)) == d);
  CHECK(difficultyName(Difficulty::Hard) == "hard");
  CHECK_FALSE(parseDifficulty("nightmare"));
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
  for (Difficulty d :
       {Difficulty::Peaceful, Difficulty::Normal, Difficulty::Easy, Difficulty::Hard}) {
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

TEST_CASE("hard is half as much again as normal, rounded up") {
  CHECK(scaledCap(4, Difficulty::Hard) == 6);
  CHECK(scaledCap(3, Difficulty::Hard) == 5);
  CHECK(scaledDamage(1, Difficulty::Hard) == 2);
  CHECK(scaledDamage(2, Difficulty::Hard) == 3);
  CHECK(scaledHealth(6, Difficulty::Hard) == 9);
  CHECK(scaledHealth(4, Difficulty::Hard) == 6);
  CHECK(scaledHealth(16, Difficulty::Hard) == 24);
  CHECK(scaledInterval(1000, Difficulty::Hard) == 750);
  CHECK(speedScale(Difficulty::Hard) == 1.0f);
  for (Difficulty d : {Difficulty::Peaceful, Difficulty::Easy, Difficulty::Normal}) {
    CHECK(scaledHealth(6, d) == 6);
    CHECK(scaledInterval(1000, d) == 1000);
  }
}

TEST_CASE("only normal and hard need food to heal") {
  CHECK_FALSE(needsFood(Difficulty::Peaceful));
  CHECK_FALSE(needsFood(Difficulty::Easy));
  CHECK(needsFood(Difficulty::Normal));
  CHECK(needsFood(Difficulty::Hard));
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

// ─── Seeds ─────────────────────────────────────────────────────────────

#include "core/seed.hpp"

TEST_CASE("seeds from text: numbers as-is, words hashed") {
  CHECK(resolveSeed("12345") == 12345u);
  CHECK(resolveSeed("  42 ") == 42u);
  CHECK(resolveSeed("-1") == 0xFFFFFFFFu);
  CHECK(resolveSeed("castle") == resolveSeed("castle"));
  CHECK(resolveSeed("castle") != resolveSeed("Castle"));
  CHECK(resolveSeed("99999999999999999999999") == resolveSeed("99999999999999999999999"));
}

TEST_CASE("noise offsets stay within 5000 units") {
  for (uint32_t seed : {0u, 1u, 4999u, 5000u, 123456789u, 0xFFFFFFFFu}) {
    glm::vec2 o = noiseOffsetFor(seed);
    CHECK(o.x >= 0.0f);
    CHECK(o.x < 5000.0f);
    CHECK(o.y >= 0.0f);
    CHECK(o.y < 5000.0f);
  }
  CHECK(noiseOffsetFor(5001) == glm::vec2(1.0f, 1.0f));
}

TEST_CASE("reset to defaults keeps the world, name and port") {
  Settings s;
  s.world_name = "castle";
  s.world_seed = 42;
  s.noise_offset = {1.0f, 2.0f};
  s.player_name = "pat";
  s.server_port = 50000;
  s.window_width = 1280;
  s.time_scale = 720.0f;
  s.fog_start = 300.0f;
  s.clouds_enabled = false;
  s.shadow_map_size = 1024;
  s.difficulty = Difficulty::Normal;
  s.render_distance = 64;

  Settings d = defaultsKeepingIdentity(s);
  Settings fresh;
  CHECK(d.world_name == "castle");
  CHECK(d.world_seed == 42u);
  CHECK(d.noise_offset == glm::vec2(1.0f, 2.0f));
  CHECK(d.player_name == "pat");
  CHECK(d.server_port == 50000);
  CHECK(d.window_width == 1280);
  CHECK(d.time_scale == fresh.time_scale);
  CHECK(d.fog_start == fresh.fog_start);
  CHECK(d.clouds_enabled == fresh.clouds_enabled);
  CHECK(d.shadow_map_size == fresh.shadow_map_size);
  CHECK(d.difficulty == fresh.difficulty);
  CHECK(d.render_distance == fresh.render_distance);
}
