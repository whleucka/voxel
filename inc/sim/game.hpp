#pragma once

#include "entity/entity_store.hpp"
#include "entity/spawner.hpp"
#include "sim/connection.hpp"
#include "sim/player_state.hpp"
#include "world/game_clock.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include <cstdint>
#include <deque>
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
  // A world with a save directory saves itself this often, so a crash costs
  // at most these last minutes.
  static constexpr int kAutosaveTicks = 5 * 60 * 20; // five minutes
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
  // Beds
  static constexpr float kSleepEnemyRange = 8.0f; // no sleeping with one this close
  static constexpr int kSleepMs = 2000;           // everyone this long in bed: morning
  static constexpr float kWakeTime = 6.5f * 3600; // 6:30
  // Wild plants
  static constexpr int kBerriesPicked = 2;
  static constexpr int kBushRipenMs = 5 * 60 * 1000; // a picked bush ripens again
  static constexpr int kCakeSliceHeal = 4;
  static constexpr int kCompostSprouts = 4;        // plants it tries to sprout
  static constexpr int kCompostReach = 2;          // round the block it's used on
  static constexpr int kPrickDamage = 1;
  static constexpr int kPrickCooldownMs = 1000;
  static constexpr float kPrickRange = 0.05f;      // gap between player and cactus

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
  const std::deque<ChatMessage> &chatHistory() const { return chat_history; }
  // The host's clock for chat times, in minutes since midnight. Tests can
  // swap it for a fixed one.
  uint16_t (*chat_minute)() = localMinute;
  static uint16_t localMinute();
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

  // Grows every crop and dries every patch of farmland by `ms`, as tick()
  // does once a second; tests call it to skip ahead.
  void growFarms(int ms);
  // Time until a growing crop's next stage (nothing if it isn't growing),
  // and whether farmland is known to the Game.
  std::optional<int> cropTimeLeft(glm::ivec3 pos) const;
  bool tracksSoil(glm::ivec3 pos) const { return soil_.count(pos) > 0; }

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
  void apply(Client &client, const UseBed &cmd);
  void apply(Client &client, const LeaveBed &cmd);
  void apply(Client &client, const UseItemOn &cmd);
  void apply(Client &client, const Chat &cmd);
  void apply(Client &client, const TogglePin &cmd);
  void apply(Client &client, const BoardBoat &cmd);
  void apply(Client &client, const LeaveBoat &cmd);
  // Boats: putting one on the water, picking one up, who sits where, and
  // getting out.
  void placeBoat(Client &client, PlayerState &ps, glm::ivec3 pos);
  void pickUpBoat(Client &client, PlayerState &ps, EntityId id);
  PlayerState *riderIn(EntityId boat, int seat);
  bool overlapsBoat(const glm::ivec3 &block) const;
  // Every rider of the boat goes to their seat on it.
  void seatRiders(const Entity &boat);
  // Out of the boat, standing beside it.
  void unseat(Client &client, PlayerState &ps);
  // Where to stand after getting out of a boat at `boat` facing `yaw`: dry
  // land beside it, else the water beside it, else straight up.
  glm::vec3 besideBoat(glm::vec3 boat, float yaw) const;
  // Where a player stands, for saving: a rider stands beside their boat.
  glm::vec3 standingPosition(const PlayerState &ps) const;
  // Riders who died or lost their boat get out; the rest follow their seats.
  void tickBoats();
  // Seats saved with the world whose riders haven't come back yet, by boat.
  // A returning rider sits back down if their seat is still free.
  using SavedSeats = std::array<WorldSave::BoatRecord::Seat, kBoatSeats>;
  std::map<EntityId, SavedSeats> saved_seats;
  void retakeSeat(Client &client, PlayerState &ps, bool saved_host);
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
  // A chat line to every joined player, kept in the history. from 0: news.
  void postChat(PlayerId from, const std::string &name, const std::string &text);
  // "Pat was beaten by a Zombie": once per death, for anyone newly dead.
  void announceDeaths();
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
  std::deque<ChatMessage> chat_history; // the last kChatHistory lines
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

  // Farms: every growing crop's time to its next stage, and every patch of
  // farmland's water, by block, growing whether or not their chunk is loaded.
  struct GrowingCrop { int left_ms = 0; };
  struct Soil {
    int wet_ms = 0;        // left before watered soil dries
    bool by_water = false; // water close by: as of when its chunk was last loaded
  };
  std::map<glm::ivec3, GrowingCrop, BlockLess> crops_;
  std::map<glm::ivec3, Soil, BlockLess> soil_;
  void till(Client &client, PlayerState &ps, glm::ivec3 pos);
  // Wild plants: picking a ripe berry bush (it ripens again with the crops),
  // a slice of cake, and compost on a crop, a bush, grass or sand.
  void pickBerries(Client &client, PlayerState &ps, glm::ivec3 pos);
  void eatCake(Client &client, PlayerState &ps, glm::ivec3 pos);
  void useCompost(Client &client, PlayerState &ps, glm::ivec3 pos);
  // A cactus touching a living player pricks them, at most once a second.
  void prickPlayers();
  void plant(Client &client, PlayerState &ps, glm::ivec3 pos);
  void water(Client &client, PlayerState &ps, glm::ivec3 pos);
  // The block at a farm cell, loaded or not: an unloaded one is its edit,
  // since farms are only ever made by players (nothing if it's unedited).
  std::optional<BlockType> farmBlockAt(glm::ivec3 pos) const;
  void setFarmBlock(glm::ivec3 pos, BlockType type);
  // Something went from under a crop: it pops off as its seed.
  void popCropAbove(glm::ivec3 below);
  // The block at `gone` went: plants, cactus, torches and cake it held up
  // pop off (and what they held, in turn), dropping what they drop.
  void popUnsupported(glm::ivec3 gone);
  // What breaking `b` gives, by the roll for one that's chancy (tall grass).
  std::optional<ItemId> dropOf(BlockType b);
  // A block placed on farmland packs it back into dirt.
  void packSoilUnder(glm::ivec3 placed);
  // Gives a player items, popping out at `pos` what doesn't fit.
  void giveOrDrop(PlayerState &ps, ItemStack stack, glm::ivec3 pos);
  // Doors: setting both halves at once, placing one, and popping one whose
  // support at `below` has gone (it drops, in survival).
  void setDoor(glm::ivec3 bottom, BlockType bottom_half);
  void placeDoor(Client &client, PlayerState &ps, glm::ivec3 pos);
  void popDoorAbove(glm::ivec3 below);
  // Beds: placing one (the held colour), and popping one whose support at
  // `below` has gone (both halves go; one drops, in survival).
  void placeBed(Client &client, PlayerState &ps, glm::ivec3 pos);
  void popBedAbove(glm::ivec3 below);
  // Whether the bed with its foot at `foot` is still there, loaded or not.
  bool bedStillAt(glm::ivec3 foot) const;
  // Where to stand up from the bed with its foot at `foot`: a clear spot
  // beside it, or on top of it.
  glm::vec3 besideBed(glm::ivec3 foot) const;
  // Out of bed and standing beside it.
  void wake(Client &client, PlayerState &ps);
  // Wakes anyone whose bed went, who got hurt, or whose night is over, and
  // skips to morning once every living player has slept long enough.
  void tickSleep();
  // Send SleepState to every owner whose shown state changed.
  void flushSleep();
  // Sends each player their Waypoints when home or anyone's pins change.
  void flushWaypoints();
  // Sends each player where the others are, for their map (once a second).
  void sendMapPlayers();
  // Where a player's map points home: their bed, or the world spawn.
  glm::ivec3 home(const PlayerState &ps);
  Client *clientOf(PlayerId id);
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
