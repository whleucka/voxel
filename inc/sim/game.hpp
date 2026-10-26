#pragma once

#include "entity/entity_store.hpp"
#include "entity/spawner.hpp"
#include "sim/connection.hpp"
#include "sim/player_state.hpp"
#include "world/game_clock.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
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
  // Dropped items
  static constexpr int kBreakDropDelayMs = 500;
  static constexpr int kThrowDropDelayMs = 1500;
  static constexpr float kPickupRange = 1.5f;
  static constexpr float kMergeRange = 1.0f;
  // Hitting entities
  static constexpr int kAttackCooldownMs = 400;
  static constexpr int kAttackDamage = 2; // bare hands
  static constexpr int kHurtFlashMs = 300;
  static constexpr float kKnockback = 6.0f;
  static constexpr float kKnockUp = 4.0f;
  // Feeding animals
  static constexpr int kFedCooldownMs = 60000;
  static constexpr int kHappyMs = 2000;
  static constexpr int kProductDelayMs = 1000;

  Game(); // registers natural spawning (animals and enemies)

  // Bind the game to a save directory and load it if a world exists there.
  // An existing world keeps its own seed (written to g_settings) and clock;
  // otherwise the properties seed creates a fresh one. Call before any Join.
  // Returns true if an existing world was loaded.
  bool load(const std::string &dir);
  // Write level, edits and the host player to the bound directory.
  bool save();

  // Install already-loaded save data before any client joins. The first Join
  // takes over the saved player record (single-player); later joins get a
  // fresh spawn. load() uses this; tests may call it directly.
  void restore(const WorldSave::LevelData &level, WorldEdits edits,
               std::optional<WorldSave::PlayerData> player,
               std::optional<std::vector<ItemStack>> inventory = std::nullopt);

  void addClient(ClientLink &link);

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
  EntityStore &entities() { return entities_; }
  Spawner &spawner() { return spawner_; }
  const EntityStore &entities() const { return entities_; }
  // Drives spawning, wandering and hops; tests reseed it for determinism.
  Rng &rng() { return rng_; }
  const PlayerState *findPlayer(PlayerId id) const;

  // What a player with no saved inventory starts with.
  static Inventory starterKit();

private:
  struct Client {
    ClientLink *link = nullptr;
    std::optional<PlayerId> player;
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
  // Eye-to-box distance check shared by hitting and feeding.
  bool inReachOf(const PlayerState &ps, const Entity &e) const;
  void releaseAnimalProducts();

  // Puts a stack into the world as a dropped item. Returns false (the stack is
  // lost) only if the entity cap is reached.
  bool dropItem(ItemStack stack, glm::vec3 pos, glm::vec3 velocity, int pickup_delay_ms);
  void mergeDroppedItems();
  void pickUpDroppedItems();

  PlayerState *playerFor(const Client &client);
  void reject(Client &client, std::string reason);
  void broadcast(const Event &ev);
  void sendInventory(Client &client, const PlayerState &ps);
  bool inReach(const PlayerState &ps, const glm::ivec3 &block) const;
  bool overlapsAnyPlayer(const glm::ivec3 &block) const;
  // Stream chunks around every player.
  void updateInterestPoints();
  // Send VitalsChanged to every owner whose shown vitals changed.
  void flushVitals();
  void sendEntitySnapshots();
  void respawn(Client &client, PlayerState &ps);
  glm::vec3 respawnPoint();

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
  std::string save_dir;
  std::optional<glm::vec3> respawn_point; // world spawn, found on first death
};
