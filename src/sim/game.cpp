#include "sim/game.hpp"
#include "block/block_data.hpp"
#include "core/settings.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <type_traits>

// ─── Setup ─────────────────────────────────────────────────────────────

bool Game::load(const std::string &dir) {
  save_dir = dir;

  WorldSave::LevelData level;
  if (!WorldSave::loadLevel(dir, level)) return false;

  g_settings.world_seed   = level.seed;
  g_settings.noise_offset = level.noise_offset;
  srand(level.seed);

  WorldEdits edits;
  WorldSave::loadEdits(dir, edits);
  std::optional<WorldSave::PlayerData> player;
  if (WorldSave::PlayerData pd; WorldSave::loadPlayer(dir, pd)) player = pd;

  restore(level, std::move(edits), player);
  return true;
}

bool Game::save() {
  if (save_dir.empty()) return false;

  WorldSave::LevelData level;
  level.seed         = g_settings.world_seed;
  level.noise_offset = g_settings.noise_offset;
  level.time_of_day  = clock_.time_of_day;

  // The save format holds one player: the host, who joined first. A saved
  // player that never rejoined is written back unchanged.
  WorldSave::PlayerData pd = saved_player.value_or(WorldSave::PlayerData{});
  const PlayerState *host = nullptr;
  for (const auto &[id, ps] : players)
    if (!host || id < host->id) host = &ps;
  if (host) {
    pd.position      = host->position;
    pd.yaw           = host->yaw;
    pd.pitch         = host->pitch;
    pd.selected_slot = host->selected_slot;
    pd.fly_mode      = host->fly ? 1 : 0;
  }

  if (!WorldSave::save(save_dir, level, pd, world_.snapshotEdits())) {
    std::cerr << "Failed to save world '" << g_settings.world_name << "'\n";
    return false;
  }
  std::cout << "Saved world '" << g_settings.world_name << "'\n";
  return true;
}

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

void Game::broadcast(const Event &ev) {
  for (Client &client : clients) client.link->deliver(ev);
}

// The client's raycast measures reach to the face it hit; the block's centre
// can be up to half a block-diagonal further than that.
bool Game::inReach(const PlayerState &ps, const glm::ivec3 &block) const {
  constexpr float kHalfBlockDiagonal = 0.8660254f; // sqrt(3) / 2
  glm::vec3 centre = glm::vec3(block) + 0.5f;
  return glm::distance(ps.eyePosition(), centre) <=
         kPlayerReach + kHalfBlockDiagonal;
}

bool Game::overlapsAnyPlayer(const glm::ivec3 &block) const {
  const glm::vec3 bmin(block);
  const glm::vec3 bmax = bmin + 1.0f;
  for (const auto &[id, ps] : players) {
    const glm::vec3 pmin = ps.position - glm::vec3(kPlayerHalfWidth, 0.0f, kPlayerHalfWidth);
    const glm::vec3 pmax = ps.position + glm::vec3(kPlayerHalfWidth, ps.height(), kPlayerHalfWidth);
    bool overlaps = pmin.x < bmax.x && pmax.x > bmin.x &&
                    pmin.y < bmax.y && pmax.y > bmin.y &&
                    pmin.z < bmax.z && pmax.z > bmin.z;
    if (overlaps) return true;
  }
  return false;
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
                      std::is_same_v<T, BreakBlock> ||
                      std::is_same_v<T, PlaceBlock> ||
                      std::is_same_v<T, SetTimeOfDay> ||
                      std::is_same_v<T, Save>)
          apply(client, c);
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

void Game::apply(Client &client, const BreakBlock &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  if (cmd.pos.y < 0 || cmd.pos.y >= kChunkHeight)
    return reject(client, "break: outside world height");
  if (!inReach(*ps, cmd.pos)) return reject(client, "break: out of reach");

  BlockType target = world_.getBlockAt(glm::vec3(cmd.pos) + 0.5f);
  if (target == BlockType::AIR) return reject(client, "break: nothing there");
  if (target == BlockType::BEDROCK || target == BlockType::WATER)
    return reject(client, "break: unbreakable block");

  world_.setBlockAt(cmd.pos, BlockType::AIR);
  broadcast(BlockChanged{cmd.pos, BlockType::AIR});
}

void Game::apply(Client &client, const PlaceBlock &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  if (cmd.type == BlockType::AIR) return reject(client, "place: no block");
  if (cmd.pos.y < 0 || cmd.pos.y >= kChunkHeight)
    return reject(client, "place: outside world height");
  if (!inReach(*ps, cmd.pos)) return reject(client, "place: out of reach");

  // An unloaded cell reads as AIR but World::setBlockAt would drop the edit.
  int cx = static_cast<int>(std::floor(static_cast<float>(cmd.pos.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(cmd.pos.z) / kChunkDepth));
  if (!world_.getChunk(cx, cz)) return reject(client, "place: chunk not loaded");

  // Only air or liquid can be replaced.
  BlockType existing = world_.getBlockAt(glm::vec3(cmd.pos) + 0.5f);
  if (existing != BlockType::AIR && !isLiquid(existing))
    return reject(client, "place: cell occupied");
  if (overlapsAnyPlayer(cmd.pos))
    return reject(client, "place: inside a player");

  world_.setBlockAt(cmd.pos, cmd.type);
  broadcast(BlockChanged{cmd.pos, cmd.type});
}

void Game::apply(Client &, const SetTimeOfDay &cmd) {
  clock_.time_of_day = std::fmod(std::max(cmd.seconds, 0.0f), 86400.0f);
}

void Game::apply(Client &, const Save &) { save(); }
