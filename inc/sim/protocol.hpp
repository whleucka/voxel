#pragma once

#include "block/block_type.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <variant>

// Messages between a client (Engine) and the authoritative Game. Plain structs
// for now; the multiplayer module adds a wire format.

using PlayerId = uint32_t;

// ─── Commands (client → sim) ───────────────────────────────────────────

struct Join {};

// Movement is client-authoritative: the client runs physics and reports where
// it ended up. Fine on a home LAN, and it keeps movement smooth.
struct MovePlayer {
  glm::vec3 position{0.0f};
  float yaw = -90.0f;
  float pitch = 0.0f;
  bool fly = false;
  bool crouch = false;
};

struct BreakBlock { glm::ivec3 pos{0}; };
struct PlaceBlock { glm::ivec3 pos{0}; BlockType type = BlockType::AIR; };
struct SelectSlot { int slot = 0; };
struct Save {};
struct SetTimeOfDay { float seconds = 0.0f; }; // debug panel

using Command = std::variant<Join, MovePlayer, BreakBlock, PlaceBlock,
                             SelectSlot, Save, SetTimeOfDay>;

// ─── Events (sim → client) ─────────────────────────────────────────────

struct Joined {
  PlayerId id = 0;
  glm::vec3 position{0.0f};
  float yaw = -90.0f;
  float pitch = 0.0f;
  int slot = 0;
  bool fly = false;
};

struct BlockChanged { glm::ivec3 pos{0}; BlockType type = BlockType::AIR; };
struct PlayerJoined { PlayerId id = 0; };
struct PlayerLeft { PlayerId id = 0; };
struct CommandRejected { std::string reason; };

using Event = std::variant<Joined, BlockChanged, PlayerJoined, PlayerLeft,
                           CommandRejected>;
