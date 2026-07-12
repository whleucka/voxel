#pragma once

#include "chunk/chunk.hpp" // ChunkKey, ChunkKeyHash
#include "robin_hood/robin_hood.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <string>

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
};

// player.dat — player state restored after the world is generated.
struct PlayerData {
  glm::vec3 position{0.0f};
  float     yaw = -90.0f;
  float     pitch = 0.0f;
  int32_t   selected_slot = 0;
  uint8_t   fly_mode = 0;
};

// Resolves a world name to its directory, e.g. "world" -> "saves/world".
std::string worldDir(const std::string &name);

// Loaders return false if the file is missing or has a bad/mismatched header;
// the caller then treats it as a fresh world.
bool loadLevel(const std::string &dir, LevelData &out);
bool loadPlayer(const std::string &dir, PlayerData &out);
bool loadEdits(const std::string &dir, WorldEdits &out);

// Writes level.dat, player.dat and edits.dat into dir (created if needed).
bool save(const std::string &dir, const LevelData &level,
          const PlayerData &player, const WorldEdits &edits);

} // namespace WorldSave
