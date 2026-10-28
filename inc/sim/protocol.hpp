#pragma once

#include "world/worldgen_version.hpp"
#include "block/block_type.hpp"
#include "core/game_mode.hpp"
#include "entity/entity_type.hpp"
#include "item/chest.hpp"
#include "inventory/inventory.hpp"
#include "world/block_edit.hpp"
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

// The first command on every link. An empty name becomes "Player<id>".
struct Join { std::string name; };

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
  // The latest Respawned counter this client has seen. Moves sent before a
  // respawn arrive with the old one and are ignored.
  uint32_t respawn = 0;
};

struct BreakBlock { glm::ivec3 pos{0}; };
// Places the block item held in the selected hotbar slot; the Game decides
// what that is, not the client.
struct PlaceBlock { glm::ivec3 pos{0}; };
struct SelectSlot { int slot = 0; };
struct Save {};
struct SetTimeOfDay { float seconds = 0.0f; }; // debug panel
struct GiveItem { ItemId id = ItemId::NONE; uint16_t count = 0; }; // debug panel
// Right-click on an entity: feed an animal its favourite (held) block.
struct InteractEntity { EntityId id = 0; };
// Eat one of the held food item.
struct EatItem {};
// Debug panel: put an entity a few blocks in front of the player.
struct SpawnDebugEntity { EntityKind kind = EntityKind::TestCreature; };
// Hit an entity the client is aiming at. The Game checks reach and cooldown.
struct AttackEntity { EntityId id = 0; };
// Throw the held item (one, or the whole stack) forward from the eyes.
struct DropItem { bool whole_stack = false; };
// Move count items (0 = the whole stack) between inventory slots; see
// Inventory::move for the merge/swap rules.
// Slots are inventory slots, or these for the container the mover has open.
constexpr int kContainerSlot0 = 1000;
constexpr int kFurnaceInput = kContainerSlot0, kFurnaceFuel = kContainerSlot0 + 1,
              kFurnaceOutput = kContainerSlot0 + 2;
// A chest's slots are kContainerSlot0 + 0 .. kChestSlots - 1.
struct MoveItems { int from = 0; int to = 0; uint16_t count = 0; };

// Survival: the left button went down on a block (or the aim moved to a new
// one). The BreakBlock that follows is checked against the time since.
struct StartBreak { glm::ivec3 pos{0}; };

// Look inside a furnace or chest (in reach) until CloseContainer or walking
// away. On a treasure chest it unlocks it, using up the held Treasure Key.
struct OpenContainer { glm::ivec3 pos{0}; };
struct CloseContainer {};

// Make a recipe (its index in recipes()) once, or as many times as fit.
// The Game checks it's discovered, the ingredients, room for the result,
// and a Crafting Table in reach when the recipe needs one.
struct Craft { uint16_t recipe = 0; bool all = false; };

// With a fishing rod held: throw the bobber along `dir` (the aim), replacing
// any line already out. ReelIn brings it back, with a catch if it was biting.
struct CastLine { glm::vec3 dir{0.0f, 0.0f, -1.0f}; };
struct ReelIn {};
// Opens or closes the door at `pos` (either half). The Game swings both.
struct UseDoor { glm::ivec3 pos{0}; };

using Command = std::variant<Join, MovePlayer, BreakBlock, PlaceBlock,
                             SelectSlot, Save, SetTimeOfDay, GiveItem,
                             MoveItems, DropItem, AttackEntity,
                             SpawnDebugEntity, InteractEntity, EatItem, Craft,
                             OpenContainer, CloseContainer, StartBreak,
                             CastLine, ReelIn, UseDoor>;

// ─── Events (sim → client) ─────────────────────────────────────────────

struct Joined {
  PlayerId id = 0;
  glm::vec3 position{0.0f};
  float yaw = -90.0f;
  float pitch = 0.0f;
  int slot = 0;
  bool fly = false;
  GameMode mode = GameMode::Survival; // the world's
};

// Sent to a guest before Joined: everything needed to build its own copy of
// the host's world (terrain comes from the seed; edits are replayed on top).
struct Welcome {
  uint32_t seed = 0;
  glm::vec2 noise_offset{0.0f};
  float time_of_day = 0.0f; // seconds
  float time_scale = 1.0f;
  std::vector<BlockEdit> edits;
  uint8_t worldgen = worldgen::kOriginal; // the host world's generator
};

// The host's clock, once a second, so every guest's sky matches.
struct TimeSync { float time_of_day = 0.0f; float time_scale = 1.0f; };

struct BlockChanged { glm::ivec3 pos{0}; BlockType type = BlockType::AIR; };
// Another player joined or left (never sent about yourself).
struct PlayerJoined { PlayerId id = 0; std::string name; };
struct PlayerLeft { PlayerId id = 0; };
struct CommandRejected { std::string reason; };
// The link to the host is gone: the host quit (it sends this first) or the
// connection failed (the guest's NetConnection reports it). Back to the menu.
struct Disconnected { std::string reason; };

// The Join was refused (name in use, game full); the link is then closed.
struct JoinRefused { std::string reason; };

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
struct Respawned { glm::vec3 position{0.0f}; uint32_t respawn = 0; };

// An enemy hit this player: the client adds this to its velocity (movement is
// client-side, so the Game can't push the player itself).
struct KnockedBack { glm::vec3 velocity{0.0f}; };

// What a client needs to draw one entity.
struct EntityView {
  EntityId id = 0;
  EntityKind kind = EntityKind::TestCreature;
  glm::vec3 position{0.0f}; // feet
  float yaw = 0.0f;
  ItemId item = ItemId::NONE; // dropped items: what to draw
  bool hurt = false;          // flash red
  bool happy = false;         // fed animal: show a heart
  bool burning = false;       // set alight: glow orange
  PlayerId owner = 0;         // bobbers: who's fishing (draws the line)
  bool biting = false;        // bobbers: dipped under
  bool moving = false;        // set client-side by the interpolator
};

// Another living player, as a client draws them.
struct PlayerView {
  PlayerId id = 0;
  std::string name;
  glm::vec3 position{0.0f}; // feet
  float yaw = -90.0f;       // camera convention (degrees)
  float pitch = 0.0f;
  bool crouch = false;
  bool moving = false;      // set client-side by the interpolator
};

// Every entity and other player within range of this player, sent every tick.
// One that isn't listed is gone (or out of range). Never includes yourself.
struct EntitySnapshot {
  uint32_t tick = 0;
  std::vector<EntityView> entities;
  std::vector<PlayerView> players = {};
};

// Items the owner has now had for the first time (all of them, on joining):
// they unlock recipes. The client keeps the running set.
struct KnownItems { std::vector<ItemId> ids; };

// The owner's tool wore out and is gone.
struct ToolBroke { ItemId item = ItemId::NONE; };

// The owner reeled something in (it's in their inventory, or at their feet).
struct Caught { ItemId item = ItemId::NONE; };

// The open furnace, to everyone looking in: when they open it, when its
// slots change, and a few times a second while it burns. `open` false: it's
// closed for them (out of reach, or the furnace is gone).
enum class ContainerKind : uint8_t { Furnace, Chest };
struct ContainerView {
  glm::ivec3 pos{0};
  bool open = true;
  ContainerKind kind = ContainerKind::Furnace;
  std::vector<ItemStack> slots; // a chest's kChestSlots
  ItemStack input, fuel, output; // a furnace's
  uint8_t burn = 0; // fuel left in the current burn, 0-255
  uint8_t cook = 0; // progress on the item cooking, 0-255
};

using Event = std::variant<Joined, BlockChanged, PlayerJoined, PlayerLeft,
                           CommandRejected, InventoryContents, VitalsChanged,
                           Respawned, EntitySnapshot, KnockedBack, JoinRefused,
                           Welcome, TimeSync, Disconnected, KnownItems,
                           ContainerView, ToolBroke, Caught>;
