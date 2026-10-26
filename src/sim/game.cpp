#include "sim/game.hpp"
#include "core/settings.hpp"
#include <cmath>
#include <type_traits>

// ─── Setup ─────────────────────────────────────────────────────────────

void Game::restore(const WorldSave::LevelData &level, WorldEdits edits,
                   std::optional<WorldSave::PlayerData> player) {
  clock_.time_of_day = level.time_of_day;
  world_.setLoadedEdits(std::move(edits));
  saved_player = std::move(player);
}

void Game::addClient(ClientLink &link) { clients.push_back({&link, {}}); }

const PlayerState *Game::findPlayer(PlayerId id) const {
  auto it = players.find(id);
  return it == players.end() ? nullptr : &it->second;
}

PlayerState *Game::playerFor(const Client &client) {
  if (!client.player) return nullptr;
  auto it = players.find(*client.player);
  return it == players.end() ? nullptr : &it->second;
}

void Game::reject(Client &client, std::string reason) {
  client.link->deliver(CommandRejected{std::move(reason)});
}

// ─── Loop ──────────────────────────────────────────────────────────────

void Game::pump() {
  std::vector<Command> cmds;
  for (Client &client : clients) {
    cmds.clear();
    client.link->drain(cmds);
    for (const Command &cmd : cmds) apply(client, cmd);
  }
  updateInterestPoints();
}

void Game::updateInterestPoints() {
  std::vector<glm::vec3> points;
  points.reserve(players.size());
  for (const auto &[id, ps] : players) points.push_back(ps.position);
  world_.setInterestPoints(points);
}

int Game::advance(float dt) {
  tick_accumulator += dt;
  int ticks = 0;
  while (tick_accumulator >= kTickSeconds) {
    tick_accumulator -= kTickSeconds;
    tick();
    ++ticks;
  }
  return ticks;
}

void Game::tick() {
  clock_.scale = g_settings.time_scale;
  clock_.update(kTickSeconds);
}

// ─── Commands ──────────────────────────────────────────────────────────

void Game::apply(Client &client, const Command &cmd) {
  std::visit(
      [&](const auto &c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, Join> ||
                      std::is_same_v<T, MovePlayer> ||
                      std::is_same_v<T, SelectSlot> ||
                      std::is_same_v<T, SetTimeOfDay>)
          apply(client, c);
        // Remaining commands are routed through Game in later tasks.
      },
      cmd);
}

void Game::apply(Client &client, const Join &) {
  if (client.player) return; // already joined

  PlayerState ps;
  ps.id = next_player_id++;
  if (saved_player) {
    ps.position = saved_player->position;
    ps.yaw = saved_player->yaw;
    ps.pitch = saved_player->pitch;
    ps.selected_slot = saved_player->selected_slot;
    ps.fly = saved_player->fly_mode != 0;
    saved_player.reset();
  } else {
    ps.position = world_.findSpawn();
  }

  players[ps.id] = ps;
  client.player = ps.id;
  client.link->deliver(Joined{ps.id, ps.position, ps.yaw, ps.pitch,
                              ps.selected_slot, ps.fly});
}

void Game::apply(Client &client, const MovePlayer &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  ps->position = cmd.position;
  ps->yaw = cmd.yaw;
  ps->pitch = cmd.pitch;
  ps->fly = cmd.fly;
  ps->crouch = cmd.crouch;
}

void Game::apply(Client &client, const SelectSlot &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  // Same rule as the client's Player: out-of-range selections are ignored.
  if (cmd.slot < 0 || cmd.slot >= kHotbarSlots)
    return reject(client, "hotbar slot out of range");
  ps->selected_slot = cmd.slot;
}

void Game::apply(Client &, const SetTimeOfDay &cmd) {
  clock_.time_of_day = std::fmod(std::max(cmd.seconds, 0.0f), 86400.0f);
}
