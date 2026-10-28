#pragma once

#include "entity/entity_store.hpp"
#include "entity/spawner.hpp"
#include "sim/connection.hpp"
#include "sim/player_state.hpp"
#include "world/game_clock.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

// The authoritative simulation. Owns the world, the clock and every player's
// state; it changes only in response to Commands from attached clients and
// reports changes back as Events.
class Game {
public:
  static constexpr float kTickSeconds = 0.05f; // 20 Hz
  static constexpr int kTickMs = 50;
  static constexpr int kRespawnDelayMs = 3000;
  static constexpr float kEntityViewRange = 96.0f;
  static constexpr size_t kMaxPlayers = 8; // host included
  static constexpr size_t kMaxNameLength = 16;
  static constexpr int kTimeSyncTicks = 20; // once a second
  // Dropped items
  static constexpr int kBreakDropDelayMs = 500;
  static constexpr int kThrowDropDelayMs = 1500;
  static constexpr float kPickupRange = 1.5f;
  static constexpr float kMergeRange = 1.0f;
  // Hitting entities
  static constexpr int kAttackCooldownMs = 400;
  static constexpr int kBurnMs = 3000;    // a ruby sword's fire: a point a second
  static constexpr int kHurtFlashMs = 300;
  static constexpr float kKnockback = 6.0f;
  static constexpr float kKnockUp = 4.0f;
  // Feeding animals
  static constexpr int kFedCooldownMs = 60000;
  static constexpr int kHappyMs = 2000;
  static constexpr int kProductDelayMs = 1000;
  // Enemies hitting players
  static constexpr float kEnemyContactRange = 0.3f; // gap between the boxes
  static constexpr int kEnemyAttackCooldownMs = 1000;
  static constexpr float kEnemyKnockback = 7.0f;
  static constexpr float kEnemyKnockUp = 5.0f;

  Game(); // registers natural spawning (animals and enemies)

  // Bind the game to a save directory and load it if a world exists there.
  // An existing world keeps its own seed (written to g_settings) and clock;
  // otherwise the properties seed creates a fresh one. Call before any Join.
  // Returns true if an existing world was loaded.
  bool load(const std::string &dir);
  // Write level, edits, the host player and every guest record to the bound
  // directory.
  bool save();

  // Install already-loaded save data before any client joins. The first Join
  // takes over the saved player record (single-player); later joins get a
  // fresh spawn. load() uses this; tests may call it directly.
  void restore(const WorldSave::LevelData &level, WorldEdits edits,
               std::optional<WorldSave::PlayerData> player,
               std::optional<std::vector<ItemStack>> inventory = std::nullopt);

  // The host plays in-process and may use the debug-panel cheats; guests
  // arrive over the network.
  enum class Role { Host, Guest };
  void addClient(ClientLink &link, Role role = Role::Host);
  // The link is going away (disconnect or quit): its player leaves the game.
  void removeClient(ClientLink &link);
  size_t playerCount() const { return players.size(); }

  // 1-16 letters, digits, '_' or '-'.
  static bool validName(std::string_view name);

  // Apply every pending command. Called once per frame, so edits land with no
  // added latency; only time-based state waits for tick().
  void pump();
  // Run as many fixed ticks as dt (plus the carried remainder) covers.
  // Returns the number of ticks run.
  int advance(float dt);
  void tick();

  World &world() { return world_; }
  const World &world() const { return world_; }
  const GameClock &clock() const { return clock_; }
  GameMode mode() const { return mode_; }
  ModeRules rules() const { return rulesFor(mode_); }
  // The world's mode normally comes from its level.dat (load/restore).
  void setMode(GameMode mode) { mode_ = mode; }
  EntityStore &entities() { return entities_; }
  Spawner &spawner() { return spawner_; }
  const EntityStore &entities() const { return entities_; }
  // Drives spawning, wandering and hops; tests reseed it for determinism.
  Rng &rng() { return rng_; }
  const PlayerState *findPlayer(PlayerId id) const;
  PlayerState *findPlayer(PlayerId id) {
    return const_cast<PlayerState *>(std::as_const(*this).findPlayer(id));
  }

  // The furnace at a block, if there is one.
  const Furnace *furnaceAt(glm::ivec3 pos) const;
  Furnace *furnaceAt(glm::ivec3 pos) {
    return const_cast<Furnace *>(std::as_const(*this).furnaceAt(pos));
  }
  size_t furnaceCount() const { return furnaces_.size(); }
  // The chest at a block, if there is one.
  const Chest *chestAt(glm::ivec3 pos) const;
  Chest *chestAt(glm::ivec3 pos) {
    return const_cast<Chest *>(std::as_const(*this).chestAt(pos));
  }

  // A Crafting Table close enough to an eye at `eye` to use: the rule the
  // Game checks crafts with, shared with the client's crafting panel.
  static bool craftingTableWithinReach(const World &world, glm::vec3 eye);
  // A guest's saved state, if they've played here before.
  const WorldSave::GuestRecord *guestRecord(const std::string &name) const;

  // What a player with no saved inventory starts with.
  static Inventory starterKit();

private:
  struct Client {
    ClientLink *link = nullptr;
    std::optional<PlayerId> player;
    Role role = Role::Host;
  };

  void apply(Client &client, const Command &cmd);
  void apply(Client &client, const Join &cmd);
  void apply(Client &client, const MovePlayer &cmd);
  void apply(Client &client, const SelectSlot &cmd);
  void apply(Client &client, const BreakBlock &cmd);
  void apply(Client &client, const PlaceBlock &cmd);
  void apply(Client &client, const SetTimeOfDay &cmd);
  void apply(Client &client, const Save &cmd);
  void apply(Client &client, const GiveItem &cmd);
  void apply(Client &client, const MoveItems &cmd);
  void apply(Client &client, const DropItem &cmd);
  void apply(Client &client, const AttackEntity &cmd);
  void apply(Client &client, const SpawnDebugEntity &cmd);
  void apply(Client &client, const InteractEntity &cmd);
  void apply(Client &client, const EatItem &cmd);
  void apply(Client &client, const Craft &cmd);
  void apply(Client &client, const OpenContainer &cmd);
  void apply(Client &client, const CloseContainer &cmd);
  void apply(Client &client, const StartBreak &cmd);
  void apply(Client &client, const CastLine &cmd);
  void apply(Client &client, const ReelIn &cmd);
  void apply(Client &client, const UseDoor &cmd);
  // Bobbers float, bites come and go, and lines break.
  void tickFishing();
  // Takes the player's bobber out of the world and ends their fishing.
  void endLine(PlayerState &ps);
  // A break may come this much before its time (network and tick jitter).
  static constexpr int kBreakSlackMs = 150;
  uint64_t nowMs() const { return static_cast<uint64_t>(tick_count) * kTickMs; }
  // Survival: the held tool spends a use, and breaks when it's used up.
  // False if nothing tool-like was held (so nothing changed).
  bool wearHeldTool(Client &client, PlayerState &ps);
  // The held item spends a use, if it wears (tools, the rod).
  bool spendUse(Client &client, PlayerState &ps);
  // Without this, an alternative with no overload above would convert back
  // to a Command and recurse forever; with it, that's a compile error.
  template <class T> void apply(Client &, const T &) = delete;
  // The furnace the player has open, if it's still there and in reach.
  Furnace *openFurnace(const PlayerState &ps);
  Chest *openChest(const PlayerState &ps);
  void sendContainer(Client &client, glm::ivec3 pos);
  // Every player looking into the furnace at `pos` gets its current state.
  void sendContainerToViewers(glm::ivec3 pos);
  void closeContainer(Client &client, PlayerState &ps);
  // Eye-to-box distance check shared by hitting and feeding.
  bool inReachOf(const PlayerState &ps, const Entity &e) const;
  void releaseAnimalProducts();
  // Peaceful clears enemies; daylight clears surface enemies under the sky.
  void removeEnemies();
  // Gone: its drops pop out and it's removed.
  void killEntity(EntityId id);
  void burnEntities();
  void enemyAttacks();

  // Puts a stack into the world as a dropped item. Returns false (the stack is
  // lost) only if the entity cap is reached.
  bool dropItem(ItemStack stack, glm::vec3 pos, glm::vec3 velocity, int pickup_delay_ms);
  void mergeDroppedItems();
  void pickUpDroppedItems();

  PlayerState *playerFor(const Client &client);
  void reject(Client &client, std::string reason);
  bool hostOnly(Client &client, std::string_view what);
  void broadcast(const Event &ev);
  // Sends the inventory, and notes any item the player hasn't had before.
  void sendInventory(Client &client, PlayerState &ps);
  bool inReach(const PlayerState &ps, const glm::ivec3 &block) const;
  // A Crafting Table within the player's reach (the same rule as placing).
  bool tableInReach(const PlayerState &ps) const;

  bool overlapsAnyPlayer(const glm::ivec3 &block) const;
  // Stream chunks around every player.
  void updateInterestPoints();
  // Send VitalsChanged to every owner whose shown vitals changed.
  void flushVitals();
  void sendEntitySnapshots();
  void respawn(Client &client, PlayerState &ps);
  glm::vec3 respawnPoint();
  // Out of the world (dug through the bedrock, or fell before the ground
  // loaded): back on top of the same column, unharmed.
  void rescueFromVoid(Client &client, PlayerState &ps);
  static constexpr float kVoidY = -32.0f; // below the bedrock floor

  World world_;
  GameClock clock_;
  EntityStore entities_;
  Spawner spawner_;
  Rng rng_{0x5eed};
  uint32_t tick_count = 0;
  // Whole microseconds, so N ticks' worth of frame time runs exactly N ticks.
  int64_t tick_accumulator_us = 0;

  std::vector<Client> clients;
  std::unordered_map<PlayerId, PlayerState> players;
  PlayerId next_player_id = 1;
  std::optional<WorldSave::PlayerData> saved_player;
  std::optional<std::vector<ItemStack>> saved_inventory;
  std::optional<std::vector<ItemId>> saved_discovered; // the host's, like saved_inventory

  // Every furnace in the world, by block, ticking whether or not its chunk
  // is loaded.
  struct BlockLess {
    bool operator()(const glm::ivec3 &a, const glm::ivec3 &b) const {
      return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
    }
  };
  std::map<glm::ivec3, Furnace, BlockLess> furnaces_;
  void tickFurnaces();
  // A furnace block went away: its contents pop out where it stood.
  void spillFurnace(glm::ivec3 pos);
  std::map<glm::ivec3, Chest, BlockLess> chests_;
  void spillChest(glm::ivec3 pos);
  // Doors: setting both halves at once, placing one, and popping one whose
  // support at `below` has gone (it drops, in survival).
  void setDoor(glm::ivec3 bottom, BlockType bottom_half);
  void placeDoor(Client &client, PlayerState &ps, glm::ivec3 pos);
  void popDoorAbove(glm::ivec3 below);
  // Beds: placing one (the held colour), and popping one whose support at
  // `below` has gone (both halves go; one drops, in survival).
  void placeBed(Client &client, PlayerState &ps, glm::ivec3 pos);
  void popBedAbove(glm::ivec3 below);
  // A Treasure Key held: it's used up and the chest is filled with loot.
  // False (nothing changed) without a key.
  bool unlockTreasure(Client &client, PlayerState &ps, glm::ivec3 pos);
  // Swaps a furnace block for its lit or unlit look, loaded chunk or not.
  void showFurnaceLit(glm::ivec3 pos, bool lit);
  // Guests who have played in this world, by name (players/<name>.dat).
  std::map<std::string, WorldSave::GuestRecord> guest_records;
  void storeGuest(const PlayerState &ps);
  std::string save_dir;
  // This world's seed, fixed when it's created or loaded (sent in Welcome).
  uint32_t seed_ = 0;
  glm::vec2 noise_offset_{0.0f};
  uint8_t worldgen_ = 0;
  GameMode mode_ = GameMode::Survival;
  std::optional<glm::vec3> respawn_point; // world spawn, found on first death
};
