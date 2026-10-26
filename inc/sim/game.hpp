#pragma once

#include "sim/connection.hpp"
#include "sim/player_state.hpp"
#include "world/game_clock.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

// The authoritative simulation. Owns the world, the clock and every player's
// state; it changes only in response to Commands from attached clients and
// reports changes back as Events.
class Game {
public:
  static constexpr float kTickSeconds = 0.05f; // 20 Hz

  // Install a loaded save before any client joins. The first Join takes over
  // the saved player record (single-player); later joins get a fresh spawn.
  void restore(const WorldSave::LevelData &level, WorldEdits edits,
               std::optional<WorldSave::PlayerData> player);

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
  const PlayerState *findPlayer(PlayerId id) const;

private:
  struct Client {
    ClientLink *link = nullptr;
    std::optional<PlayerId> player;
  };

  void apply(Client &client, const Command &cmd);
  void apply(Client &client, const Join &cmd);
  void apply(Client &client, const MovePlayer &cmd);
  void apply(Client &client, const SelectSlot &cmd);
  void apply(Client &client, const SetTimeOfDay &cmd);

  PlayerState *playerFor(const Client &client);
  void reject(Client &client, std::string reason);
  // Stream chunks around every player.
  void updateInterestPoints();

  World world_;
  GameClock clock_;
  float tick_accumulator = 0.0f;

  std::vector<Client> clients;
  std::unordered_map<PlayerId, PlayerState> players;
  PlayerId next_player_id = 1;
  std::optional<WorldSave::PlayerData> saved_player;
};
