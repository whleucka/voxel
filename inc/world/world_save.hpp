#pragma once

#include "item/chest.hpp"
#include "chunk/chunk.hpp" // ChunkKey, ChunkKeyHash
#include "core/game_mode.hpp"
#include "item/item_stack.hpp"
#include "item/smelting.hpp"
#include "robin_hood/robin_hood.h"
#include "world/pin.hpp"
#include "world/worldgen_version.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <vector>

// Player-made block edits, keyed by chunk. Terrain itself is regenerated from
// the world seed, so only these deltas need to be persisted.
//   key:   linear block index within a chunk (x + W*(z + D*y))
//   value: BlockType stored as its underlying uint8
using ChunkEdits = robin_hood::unordered_map<uint32_t, uint8_t>;
using WorldEdits =
    robin_hood::unordered_map<ChunkKey, ChunkEdits, ChunkKeyHash>;

namespace WorldSave {

// level.dat — everything needed to reconstruct the world from its seed.
struct LevelData {
  uint32_t  seed = 0;
  glm::vec2 noise_offset{0.0f};
  float     time_of_day = 12 * 3600.0f;
  // A trailing byte: files written before modes existed load as Survival,
  // and older builds stop reading before it.
  GameMode  mode = GameMode::Survival;
  // Another trailing byte: files written before caves load as the original
  // generator, so their terrain doesn't change.
  uint8_t   worldgen = worldgen::kLatest;
};

// player.dat — player state restored after the world is generated.
struct PlayerData {
  glm::vec3 position{0.0f};
  float     yaw = -90.0f;
  float     pitch = 0.0f;
  int32_t   selected_slot = 0;
  uint8_t   fly_mode = 0;
  // The foot of the bed they respawn at. A trailing section, in player.dat
  // and in guest records: older files load with none.
  std::optional<glm::ivec3> bed;
  // Their map pins: another trailing section, after the bed.
  std::vector<Pin> pins;
};

// Resolves a world name to its directory, e.g. "world" -> "saves/world".
std::string worldDir(const std::string &name);

// Every save is written whole or not at all (a temp file renamed over the old
// one), so a crash mid-save leaves the last good file.

// Renames dir/file to dir/file.unreadable, if it is there: for a file that
// exists but won't load, so the next save can't write over what is left of it.
void setAside(const std::string &dir, const std::string &file);

// Loaders return false if the file is missing or has a bad/mismatched header;
// the caller then treats it as a fresh world.
bool loadLevel(const std::string &dir, LevelData &out);
bool loadPlayer(const std::string &dir, PlayerData &out);
bool loadEdits(const std::string &dir, WorldEdits &out);

// Writes just level.dat into dir (created if needed): a brand-new world.
bool saveLevel(const std::string &dir, const LevelData &level);

// Writes level.dat, player.dat and edits.dat into dir (created if needed).
bool save(const std::string &dir, const LevelData &level,
          const PlayerData &player, const WorldEdits &edits);

// inventory.dat — the host player's inventory slots, in slot order. Kept
// separate so the files above stay readable by older builds. Items this build
// doesn't know load as empty slots; the caller copies however many slots fit.
bool loadInventory(const std::string &dir, std::vector<ItemStack> &out);
bool saveInventory(const std::string &dir, std::span<const ItemStack> slots);

// discovered.dat — every item the host has ever had (crafting discovery).
// Worlds from before crafting have none; the Game starts it from the inventory.
bool loadDiscovered(const std::string &dir, std::vector<ItemId> &out);
bool saveDiscovered(const std::string &dir, std::span<const ItemId> ids);

// furnaces.dat — every furnace's slots and timers, by block position.
// Worlds from before smelting have none.
struct FurnaceRecord {
  glm::ivec3 pos{0};
  Furnace furnace;
};
bool loadFurnaces(const std::string &dir, std::vector<FurnaceRecord> &out);
bool saveFurnaces(const std::string &dir, std::span<const FurnaceRecord> furnaces);

// chests.dat — every chest's contents, by position.
struct ChestRecord {
  glm::ivec3 pos{0};
  Chest chest{};
};
bool loadChests(const std::string &dir, std::vector<ChestRecord> &out);
bool saveChests(const std::string &dir, std::span<const ChestRecord> chests);

// farms.dat — how far each growing crop is from its next stage, and how long
// each patch of farmland stays wet. The blocks themselves are edits; worlds
// from before farming have none.
struct CropRecord {
  glm::ivec3 pos{0};
  int32_t left_ms = 0; // until the next stage
};
struct SoilRecord {
  glm::ivec3 pos{0};
  int32_t wet_ms = 0;    // left before watered soil dries
  bool by_water = false; // water close by keeps it wet
};
struct Farms {
  std::vector<CropRecord> crops;
  std::vector<SoilRecord> soil;
};
bool loadFarms(const std::string &dir, Farms &out);
bool saveFarms(const std::string &dir, const Farms &farms);

// players/<name>.dat — a guest's state, kept in the host's world so they carry
// on where they left off. The host's own record stays in player.dat and
// inventory.dat. Names must be 1-16 of [A-Za-z0-9_-] (they're file names).
struct GuestRecord {
  PlayerData player;
  std::vector<ItemStack> inventory; // slot order
  // Every item they've ever had (crafting discovery). A trailing section:
  // records from before crafting load with none.
  std::vector<ItemId> discovered;
};
bool saveGuest(const std::string &dir, const std::string &name, const GuestRecord &rec);
// Every readable record under dir/players/, keyed by name. Missing folder:
// none yet (true). Unreadable files are skipped with a warning.
bool loadGuests(const std::string &dir, std::map<std::string, GuestRecord> &out);

} // namespace WorldSave
