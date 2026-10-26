#pragma once

#include "block/block_type.hpp"
#include "inventory/inventory.hpp"
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
// Places the block item held in the selected hotbar slot; the Game decides
// what that is, not the client.
struct PlaceBlock { glm::ivec3 pos{0}; };
struct SelectSlot { int slot = 0; };
struct Save {};
struct SetTimeOfDay { float seconds = 0.0f; }; // debug panel
struct GiveItem { ItemId id = ItemId::NONE; uint16_t count = 0; }; // debug panel

using Command = std::variant<Join, MovePlayer, BreakBlock, PlaceBlock,
                             SelectSlot, Save, SetTimeOfDay, GiveItem>;

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

// The owner's full inventory, sent after joining and after every change.
// 40 slots is small enough that a snapshot beats tracking deltas.
struct InventoryContents { Inventory::Slots slots{}; };

using Event = std::variant<Joined, BlockChanged, PlayerJoined, PlayerLeft,
                           CommandRejected, InventoryContents>;
