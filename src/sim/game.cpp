#include "sim/game.hpp"
#include "block/block_data.hpp"
#include "core/settings.hpp"
#include <cmath>
#include <iostream>

// ─── Setup ─────────────────────────────────────────────────────────────

bool Game::load(const std::string &dir) {
  save_dir = dir;

  WorldSave::LevelData level;
  if (!WorldSave::loadLevel(dir, level)) return false;

  g_settings.world_seed   = level.seed;
  g_settings.noise_offset = level.noise_offset;

  WorldEdits edits;
  WorldSave::loadEdits(dir, edits);
  std::optional<WorldSave::PlayerData> player;
  if (WorldSave::PlayerData pd; WorldSave::loadPlayer(dir, pd)) player = pd;
  std::optional<std::vector<ItemStack>> inventory;
  if (std::vector<ItemStack> inv; WorldSave::loadInventory(dir, inv))
    inventory = std::move(inv);

  restore(level, std::move(edits), player, std::move(inventory));
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

  // Same rule for the inventory; with neither a host nor a saved one there is
  // nothing to write, and the next join gets the starter kit.
  std::optional<std::vector<ItemStack>> inv = saved_inventory;
  if (host) inv.emplace(host->inventory.slots().begin(), host->inventory.slots().end());

  if (!WorldSave::save(save_dir, level, pd, world_.snapshotEdits()) ||
      (inv && !WorldSave::saveInventory(save_dir, *inv))) {
    std::cerr << "Failed to save world '" << g_settings.world_name << "'\n";
    return false;
  }
  std::cout << "Saved world '" << g_settings.world_name << "'\n";
  return true;
}

void Game::restore(const WorldSave::LevelData &level, WorldEdits edits,
                   std::optional<WorldSave::PlayerData> player,
                   std::optional<std::vector<ItemStack>> inventory) {
  clock_.time_of_day = level.time_of_day;
  world_.setLoadedEdits(std::move(edits));
  saved_player = std::move(player);
  saved_inventory = std::move(inventory);
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

void Game::sendInventory(Client &client, const PlayerState &ps) {
  client.link->deliver(InventoryContents{ps.inventory.slots()});
}

Inventory Game::starterKit() {
  Inventory kit;
  // Glowstone is the only light source and never generates naturally.
  kit.set(0, {itemForBlock(BlockType::GLOWSTONE), 8});
  return kit;
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
  flushVitals();
}

void Game::updateInterestPoints() {
  std::vector<glm::vec3> points;
  points.reserve(players.size());
  for (const auto &[id, ps] : players) points.push_back(ps.position);
  world_.setInterestPoints(points);
}

int Game::advance(float dt) {
  constexpr int64_t kTickUs = kTickMs * 1000;
  tick_accumulator_us += std::llround(static_cast<double>(dt) * 1e6);
  int ticks = 0;
  while (tick_accumulator_us >= kTickUs) {
    tick_accumulator_us -= kTickUs;
    tick();
    ++ticks;
  }
  return ticks;
}

void Game::tick() {
  clock_.scale = g_settings.time_scale;
  clock_.update(kTickSeconds);

  for (Client &client : clients) {
    PlayerState *ps = playerFor(client);
    if (!ps) continue;
    if (ps->vitals.dead()) {
      ps->dead_ms += kTickMs;
      if (ps->dead_ms >= kRespawnDelayMs) respawn(client, *ps);
    } else {
      ps->vitals.tick(kTickMs, ps->activity);
    }
  }
  flushVitals();
}

// ─── Vitals ────────────────────────────────────────────────────────────

void Game::flushVitals() {
  for (Client &client : clients) {
    PlayerState *ps = playerFor(client);
    if (!ps) continue;
    const Vitals &v = ps->vitals;
    VitalsChanged now{v.health(), v.stamina(), v.breath(), v.canSprint(), v.dead()};
    if (ps->vitals_sent == now) continue;
    ps->vitals_sent = now;
    client.link->deliver(now);
  }
}

glm::vec3 Game::respawnPoint() {
  if (!respawn_point) respawn_point = world_.findSpawn();
  return *respawn_point;
}

// Back to the world spawn with full vitals. The inventory is kept.
void Game::respawn(Client &client, PlayerState &ps) {
  ps.vitals.reset();
  ps.fall.reset();
  ps.dead_ms = 0;
  ps.position = respawnPoint();
  client.link->deliver(Respawned{ps.position});
}

// ─── Commands ──────────────────────────────────────────────────────────

// Every Command alternative needs an apply() overload; a missing one is a
// compile error here rather than a silently dropped command.
void Game::apply(Client &client, const Command &cmd) {
  std::visit([&](const auto &c) { apply(client, c); }, cmd);
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
  // The first join takes over the saved inventory, like the saved player;
  // anyone without one starts with the kit.
  if (saved_inventory) {
    for (int i = 0; i < kInventorySlots && i < static_cast<int>(saved_inventory->size()); ++i)
      ps.inventory.set(i, (*saved_inventory)[i]);
    saved_inventory.reset();
  } else {
    ps.inventory = starterKit();
  }

  players[ps.id] = ps;
  client.player = ps.id;
  client.link->deliver(Joined{ps.id, ps.position, ps.yaw, ps.pitch,
                              ps.selected_slot, ps.fly});
  sendInventory(client, ps);
}

void Game::apply(Client &client, const MovePlayer &cmd) {
  PlayerState *ps = playerFor(client);
  // The dead stay put until they respawn.
  if (!ps || ps->vitals.dead()) return;
  ps->position = cmd.position;
  ps->yaw = cmd.yaw;
  ps->pitch = cmd.pitch;
  ps->fly = cmd.fly;
  ps->crouch = cmd.crouch;
  ps->activity = {cmd.sprinting, cmd.eyes_underwater, cmd.fly};

  int fall = ps->fall.update(cmd.position.y, cmd.on_ground, cmd.in_water, cmd.fly);
  if (fall > 0) ps->vitals.damage(fall);
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
  if (!ps || ps->vitals.dead()) return;
  if (cmd.pos.y < 0 || cmd.pos.y >= kChunkHeight)
    return reject(client, "break: outside world height");
  if (!inReach(*ps, cmd.pos)) return reject(client, "break: out of reach");

  BlockType target = world_.getBlockAt(glm::vec3(cmd.pos) + 0.5f);
  if (target == BlockType::AIR) return reject(client, "break: nothing there");
  if (target == BlockType::BEDROCK || target == BlockType::WATER)
    return reject(client, "break: unbreakable block");

  world_.setBlockAt(cmd.pos, BlockType::AIR);
  broadcast(BlockChanged{cmd.pos, BlockType::AIR});

  // Survival: collect the drop. With a full inventory the block still breaks
  // and the item is lost (until dropped items exist in the world).
  if (auto drop = dropFor(target)) {
    ps->inventory.add({*drop, 1});
    sendInventory(client, *ps);
  }
}

void Game::apply(Client &client, const PlaceBlock &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;

  const ItemStack &held = ps->inventory.slot(ps->selected_slot);
  if (held.empty()) return reject(client, "place: nothing held");
  std::optional<BlockType> block = itemDef(held.id).places;
  if (!block) return reject(client, "place: held item isn't a block");

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

  world_.setBlockAt(cmd.pos, *block);
  broadcast(BlockChanged{cmd.pos, *block});
  ps->inventory.take(ps->selected_slot, 1);
  sendInventory(client, *ps);
}

void Game::apply(Client &, const SetTimeOfDay &cmd) {
  clock_.time_of_day = std::fmod(std::max(cmd.seconds, 0.0f), 86400.0f);
}

void Game::apply(Client &, const Save &) { save(); }

void Game::apply(Client &client, const GiveItem &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  if (!isValidItem(cmd.id) || cmd.count == 0)
    return reject(client, "give: unknown item");
  // Same as collecting a drop: whatever doesn't fit is lost.
  ps->inventory.add({cmd.id, cmd.count});
  sendInventory(client, *ps);
}

void Game::apply(Client &client, const MoveItems &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  if (!ps->inventory.move(cmd.from, cmd.to, cmd.count))
    return reject(client, "move: not allowed");
  sendInventory(client, *ps);
}
