#include "doctest/doctest.h"
#include "test_util.hpp"
#include "world/world_save.hpp"

TEST_CASE("level, player and edits round-trip through save/load") {
  TempDir dir;

  WorldSave::LevelData level;
  level.seed = 1234567;
  level.noise_offset = {12.0f, 34.0f};
  level.time_of_day = 4321.5f;

  WorldSave::PlayerData player;
  player.position = {10.5f, 70.0f, -3.25f};
  player.yaw = 45.0f;
  player.pitch = -12.5f;
  player.selected_slot = 4;
  player.fly_mode = 1;

  WorldEdits edits;
  edits[{0, 0}][17] = 3;
  edits[{-2, 5}][4096] = 0;
  edits[{-2, 5}][4097] = 7;

  REQUIRE(WorldSave::save(dir.path.string(), level, player, edits));

  WorldSave::LevelData level2;
  WorldSave::PlayerData player2;
  WorldEdits edits2;
  REQUIRE(WorldSave::loadLevel(dir.path.string(), level2));
  REQUIRE(WorldSave::loadPlayer(dir.path.string(), player2));
  REQUIRE(WorldSave::loadEdits(dir.path.string(), edits2));

  CHECK(level2.seed == level.seed);
  CHECK(level2.noise_offset == level.noise_offset);
  CHECK(level2.time_of_day == level.time_of_day);

  CHECK(player2.position == player.position);
  CHECK(player2.yaw == player.yaw);
  CHECK(player2.pitch == player.pitch);
  CHECK(player2.selected_slot == player.selected_slot);
  CHECK(player2.fly_mode == player.fly_mode);

  REQUIRE(edits2.size() == edits.size());
  for (const auto &[key, chunk_edits] : edits) {
    REQUIRE(edits2.count(key) == 1);
    CHECK(edits2[key].size() == chunk_edits.size());
    for (const auto &[idx, block] : chunk_edits)
      CHECK(edits2[key].at(idx) == block);
  }
}

TEST_CASE("loading from a missing directory fails cleanly") {
  WorldSave::LevelData level;
  CHECK_FALSE(WorldSave::loadLevel("/nonexistent/voxel/save", level));
}
