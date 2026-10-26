#include "doctest/doctest.h"
#include "test_util.hpp"
#include "world/world_save.hpp"
#include <fstream>

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

// ─── inventory.dat ─────────────────────────────────────────────────────

namespace {
const ItemId kDirt = itemForBlock(BlockType::DIRT);

// Writes inventory.dat by hand, for slot counts or ids the saver wouldn't make.
void writeRawInventory(const std::filesystem::path &dir,
                       const std::vector<std::pair<uint16_t, uint16_t>> &slots) {
  std::ofstream o(dir / "inventory.dat", std::ios::binary);
  uint32_t version = 1;
  o.write("VXIN", 4);
  o.write(reinterpret_cast<const char *>(&version), 4);
  uint16_t n = static_cast<uint16_t>(slots.size());
  o.write(reinterpret_cast<const char *>(&n), 2);
  for (auto [id, count] : slots) {
    o.write(reinterpret_cast<const char *>(&id), 2);
    o.write(reinterpret_cast<const char *>(&count), 2);
  }
}
} // namespace

TEST_CASE("inventory round-trips through save/load") {
  TempDir dir;
  std::vector<ItemStack> slots(40);
  slots[0] = {kDirt, 12};
  slots[9] = {ItemId::DIAMOND, 3};
  slots[39] = {itemForBlock(BlockType::GLOWSTONE), 64};

  REQUIRE(WorldSave::saveInventory(dir.path.string(), slots));
  std::vector<ItemStack> loaded;
  REQUIRE(WorldSave::loadInventory(dir.path.string(), loaded));
  CHECK(loaded == slots);
}

TEST_CASE("inventory files with a different slot count load as-is") {
  TempDir dir;
  writeRawInventory(dir.path, {{static_cast<uint16_t>(kDirt), 5}, {0, 0}, {256, 2}});
  std::vector<ItemStack> loaded;
  REQUIRE(WorldSave::loadInventory(dir.path.string(), loaded));
  REQUIRE(loaded.size() == 3);
  CHECK(loaded[0] == ItemStack{kDirt, 5});
  CHECK(loaded[1].empty());
  CHECK(loaded[2] == ItemStack{ItemId::COAL, 2});
}

TEST_CASE("unknown item ids load as empty slots") {
  TempDir dir;
  writeRawInventory(dir.path, {{9999, 5}, {static_cast<uint16_t>(BlockType::WATER), 1}});
  std::vector<ItemStack> loaded;
  REQUIRE(WorldSave::loadInventory(dir.path.string(), loaded));
  REQUIRE(loaded.size() == 2);
  CHECK(loaded[0].empty());
  CHECK(loaded[1].empty());
}

TEST_CASE("a missing or truncated inventory.dat fails to load") {
  TempDir dir;
  std::vector<ItemStack> loaded;
  CHECK_FALSE(WorldSave::loadInventory(dir.path.string(), loaded));

  {
    std::ofstream o(dir.path / "inventory.dat", std::ios::binary);
    o.write("VXIN", 4);
  }
  CHECK_FALSE(WorldSave::loadInventory(dir.path.string(), loaded));
}
