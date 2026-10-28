#include "doctest/doctest.h"
#include "block/block_type.hpp"
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
  player.bed = glm::ivec3(-40, 65, 12);

  WorldEdits edits;
  edits[{0, 0}][17] = 3;
  edits[{-2, 5}][4096] = 0;
  edits[{-2, 5}][4097] = 7;
  edits[{-2, 5}][4098] = static_cast<uint8_t>(kLastBlockType);

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
  CHECK(player2.bed == player.bed);

  // A player.dat from before beds (no trailing section) loads with none.
  const auto file = dir.path / "player.dat";
  std::filesystem::resize_file(file, std::filesystem::file_size(file) - 13);
  WorldSave::PlayerData old;
  REQUIRE(WorldSave::loadPlayer(dir.path.string(), old));
  CHECK(old.position == player.position);
  CHECK_FALSE(old.bed);

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

// ─── Guest records ─────────────────────────────────────────────────────

TEST_CASE("guest records round-trip, one file per name") {
  TempDir dir;
  WorldSave::GuestRecord bea;
  bea.player = {{1.5f, 70.0f, -3.25f}, 45.0f, -10.0f, 3, 1, glm::ivec3(2, 64, -3)};
  bea.inventory = {{ItemId::COAL, 5}, {}, {ItemId::DIAMOND, 1}};
  WorldSave::GuestRecord cy;
  cy.player.position = {-8.0f, 90.0f, 2.0f};
  REQUIRE(WorldSave::saveGuest(dir.path.string(), "Bea", bea));
  REQUIRE(WorldSave::saveGuest(dir.path.string(), "cy_2", cy));
  CHECK(std::filesystem::exists(dir.path / "players" / "Bea.dat"));

  std::map<std::string, WorldSave::GuestRecord> loaded;
  REQUIRE(WorldSave::loadGuests(dir.path.string(), loaded));
  REQUIRE(loaded.size() == 2);
  const auto &b = loaded.at("Bea");
  CHECK(b.player.position == bea.player.position);
  CHECK(b.player.yaw == 45.0f);
  CHECK(b.player.pitch == -10.0f);
  CHECK(b.player.selected_slot == 3);
  CHECK(b.player.fly_mode == 1);
  CHECK(b.player.bed == glm::ivec3(2, 64, -3));
  CHECK_FALSE(loaded.at("cy_2").player.bed);
  CHECK(b.inventory == bea.inventory);
  CHECK(loaded.at("cy_2").player.position == cy.player.position);
}

TEST_CASE("a save without a players folder has no guests yet") {
  TempDir dir;
  std::map<std::string, WorldSave::GuestRecord> loaded;
  CHECK(WorldSave::loadGuests(dir.path.string(), loaded));
  CHECK(loaded.empty());
}

TEST_CASE("guest names that aren't safe file names are refused") {
  TempDir dir;
  WorldSave::GuestRecord rec;
  CHECK_FALSE(WorldSave::saveGuest(dir.path.string(), "../evil", rec));
  CHECK_FALSE(WorldSave::saveGuest(dir.path.string(), "", rec));
  CHECK_FALSE(WorldSave::saveGuest(dir.path.string(), "a/b", rec));
}

TEST_CASE("unreadable guest files are skipped") {
  TempDir dir;
  WorldSave::GuestRecord rec;
  REQUIRE(WorldSave::saveGuest(dir.path.string(), "good", rec));
  std::ofstream(dir.path / "players" / "broken.dat") << "nope";
  std::ofstream(dir.path / "players" / "bad name.dat") << "nope";
  std::map<std::string, WorldSave::GuestRecord> loaded;
  WorldSave::loadGuests(dir.path.string(), loaded);
  CHECK(loaded.size() == 1);
  CHECK(loaded.count("good") == 1);
}

TEST_CASE("guest discoveries round-trip; a record from before crafting has none") {
  TempDir dir;
  WorldSave::GuestRecord bea;
  bea.inventory = {{ItemId::COAL, 5}};
  bea.discovered = {ItemId::COAL, ItemId::RUBY, itemForBlock(BlockType::OAK_LOG)};
  REQUIRE(WorldSave::saveGuest(dir.path.string(), "Bea", bea));
  std::map<std::string, WorldSave::GuestRecord> loaded;
  REQUIRE(WorldSave::loadGuests(dir.path.string(), loaded));
  CHECK(loaded.at("Bea").discovered == bea.discovered);

  // Drop the trailing section, as an older build wrote it.
  auto file = dir.path / "players" / "Bea.dat";
  std::filesystem::resize_file(file, std::filesystem::file_size(file) - (4 + 2 * 3));
  loaded.clear();
  REQUIRE(WorldSave::loadGuests(dir.path.string(), loaded));
  CHECK(loaded.at("Bea").inventory == bea.inventory);
  CHECK(loaded.at("Bea").discovered.empty());
}

TEST_CASE("discovered.dat round-trips and skips unknown ids") {
  TempDir dir;
  std::vector<ItemId> ids = {ItemId::STICKS, itemForBlock(BlockType::CRAFTING_TABLE),
                             static_cast<ItemId>(499)};
  REQUIRE(WorldSave::saveDiscovered(dir.path.string(), ids));
  std::vector<ItemId> back;
  REQUIRE(WorldSave::loadDiscovered(dir.path.string(), back));
  CHECK(back == std::vector<ItemId>{ItemId::STICKS, itemForBlock(BlockType::CRAFTING_TABLE)});
  CHECK_FALSE(WorldSave::loadDiscovered((dir.path / "nope").string(), back));
}

TEST_CASE("furnaces.dat round-trips slots and timers; no file, no furnaces") {
  TempDir dir;
  std::vector<WorldSave::FurnaceRecord> out;
  CHECK_FALSE(WorldSave::loadFurnaces(dir.path.string(), out));

  WorldSave::FurnaceRecord a;
  a.pos = {-12, 70, 300};
  a.furnace.input = {ItemId::RAW_IRON, 5};
  a.furnace.fuel = {ItemId::COAL, 2};
  a.furnace.output = {ItemId::IRON_INGOT, 1};
  a.furnace.burn_ms = 1234;
  a.furnace.burn_total_ms = 40000;
  a.furnace.cook_ms = 4321;
  WorldSave::FurnaceRecord b;
  b.pos = {0, 1, 2};
  std::vector<WorldSave::FurnaceRecord> in = {a, b};
  REQUIRE(WorldSave::saveFurnaces(dir.path.string(), in));
  REQUIRE(WorldSave::loadFurnaces(dir.path.string(), out));
  REQUIRE(out.size() == 2);
  CHECK(out[0].pos == a.pos);
  CHECK(out[0].furnace == a.furnace);
  CHECK(out[1].furnace == Furnace{});
}

TEST_CASE("wear is saved with inventories and guests; older files load unworn") {
  TempDir dir;
  std::vector<ItemStack> slots = {{ItemId::COAL, 1, 42}, {}, {ItemId::RUBY, 2, 0}};
  REQUIRE(WorldSave::saveInventory(dir.path.string(), slots));
  std::vector<ItemStack> back;
  REQUIRE(WorldSave::loadInventory(dir.path.string(), back));
  CHECK(back == slots);
  auto file = dir.path / "inventory.dat";
  std::filesystem::resize_file(file, std::filesystem::file_size(file) - (2 + 2 * 3));
  REQUIRE(WorldSave::loadInventory(dir.path.string(), back));
  CHECK(back[0] == ItemStack{ItemId::COAL, 1, 0});

  WorldSave::GuestRecord bea;
  bea.inventory = slots;
  REQUIRE(WorldSave::saveGuest(dir.path.string(), "Bea", bea));
  std::map<std::string, WorldSave::GuestRecord> loaded;
  REQUIRE(WorldSave::loadGuests(dir.path.string(), loaded));
  CHECK(loaded.at("Bea").inventory == slots);
}

TEST_CASE("chests.dat round-trips every slot, with wear; no file, no chests") {
  TempDir dir;
  std::vector<WorldSave::ChestRecord> out;
  CHECK_FALSE(WorldSave::loadChests(dir.path.string(), out));

  WorldSave::ChestRecord a;
  a.pos = {-40, 30, 7};
  a.chest[0] = {ItemId::DIAMOND, 2};
  a.chest[kChestSlots - 1] = {ItemId::IRON_PICKAXE, 1, 37};
  WorldSave::ChestRecord b;
  b.pos = {1, 2, 3};
  std::vector<WorldSave::ChestRecord> in = {a, b};
  REQUIRE(WorldSave::saveChests(dir.path.string(), in));
  REQUIRE(WorldSave::loadChests(dir.path.string(), out));
  REQUIRE(out.size() == 2);
  CHECK(out[0].pos == a.pos);
  CHECK(out[0].chest == a.chest);
  CHECK(out[0].chest[kChestSlots - 1].wear == 37);
  CHECK(out[1].chest == Chest{});
}
