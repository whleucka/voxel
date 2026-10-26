#pragma once

#include "block/block_type.hpp"
#include "entity/entity_type.hpp"
#include "inventory/inventory.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <variant>
#include <vector>

// Messages between a client (Engine) and the authoritative Game. Plain structs
// for now; the multiplayer module adds a wire format.

using PlayerId = uint32_t;
using EntityId = uint32_t;

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
  // Physics state the Game needs for vitals (fall damage, stamina, breath).
  bool on_ground = false;
  bool sprinting = false;
  bool in_water = false;        // feet or eyes in water
  bool eyes_underwater = false;
};

struct BreakBlock { glm::ivec3 pos{0}; };
// Places the block item held in the selected hotbar slot; the Game decides
// what that is, not the client.
struct PlaceBlock { glm::ivec3 pos{0}; };
struct SelectSlot { int slot = 0; };
struct Save {};
struct SetTimeOfDay { float seconds = 0.0f; }; // debug panel
struct GiveItem { ItemId id = ItemId::NONE; uint16_t count = 0; }; // debug panel
// Hit an entity the client is aiming at. The Game checks reach and cooldown.
struct AttackEntity { EntityId id = 0; };
// Throw the held item (one, or the whole stack) forward from the eyes.
struct DropItem { bool whole_stack = false; };
// Move count items (0 = the whole stack) between inventory slots; see
// Inventory::move for the merge/swap rules.
struct MoveItems { int from = 0; int to = 0; uint16_t count = 0; };

using Command = std::variant<Join, MovePlayer, BreakBlock, PlaceBlock,
                             SelectSlot, Save, SetTimeOfDay, GiveItem,
                             MoveItems, DropItem, AttackEntity>;

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

// The owner's vitals, sent after joining and whenever a shown value changes.
struct VitalsChanged {
  int health = 0;
  int stamina = 0;
  int breath = 0;
  bool can_sprint = true;
  bool dead = false;
  bool operator==(const VitalsChanged &) const = default;
};

// The Game moved this player (respawn); the client must jump there and stop.
struct Respawned { glm::vec3 position{0.0f}; };

// What a client needs to draw one entity.
struct EntityView {
  EntityId id = 0;
  EntityKind kind = EntityKind::TestCreature;
  glm::vec3 position{0.0f}; // feet
  float yaw = 0.0f;
  ItemId item = ItemId::NONE; // dropped items: what to draw
  bool hurt = false;          // flash red
};

// Every entity within range of this player, sent every tick. An entity that
// isn't listed is gone (or out of range).
struct EntitySnapshot {
  uint32_t tick = 0;
  std::vector<EntityView> entities;
};

using Event = std::variant<Joined, BlockChanged, PlayerJoined, PlayerLeft,
                           CommandRejected, InventoryContents, VitalsChanged,
                           Respawned, EntitySnapshot>;
