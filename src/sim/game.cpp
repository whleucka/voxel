#include "sim/game.hpp"
#include "block/block_data.hpp"
#include "item/mining.hpp"
#include "entity/animals.hpp"
#include "entity/enemies.hpp"
#include "core/settings.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>

namespace {

std::vector<ItemId> seenList(const SeenItems &seen) {
  std::vector<ItemId> out;
  for (size_t v = 0; v < seen.size(); ++v)
    if (seen.test(v)) out.push_back(static_cast<ItemId>(v));
  return out;
}

} // namespace

// ─── Setup ─────────────────────────────────────────────────────────────

Game::Game()
    : seed_(g_settings.world_seed), noise_offset_(g_settings.noise_offset),
      worldgen_(g_settings.worldgen) {
  registerAnimalSpawns(spawner_);
  registerEnemySpawns(spawner_);
}

bool Game::load(const std::string &dir) {
  save_dir = dir;
  seed_ = g_settings.world_seed;
  noise_offset_ = g_settings.noise_offset;
  worldgen_ = g_settings.worldgen;

  WorldSave::LevelData level;
  if (!WorldSave::loadLevel(dir, level)) return false;

  g_settings.world_seed   = level.seed;
  g_settings.noise_offset = level.noise_offset;
  g_settings.worldgen     = level.worldgen;
  seed_ = level.seed;
  noise_offset_ = level.noise_offset;
  worldgen_ = level.worldgen;

  WorldEdits edits;
  WorldSave::loadEdits(dir, edits);
  std::optional<WorldSave::PlayerData> player;
  if (WorldSave::PlayerData pd; WorldSave::loadPlayer(dir, pd)) player = pd;
  std::optional<std::vector<ItemStack>> inventory;
  if (std::vector<ItemStack> inv; WorldSave::loadInventory(dir, inv))
    inventory = std::move(inv);

  restore(level, std::move(edits), player, std::move(inventory));
  if (std::vector<ItemId> ids; WorldSave::loadDiscovered(dir, ids))
    saved_discovered = std::move(ids);
  furnaces_.clear();
  if (std::vector<WorldSave::FurnaceRecord> fs; WorldSave::loadFurnaces(dir, fs))
    for (const auto &r : fs) furnaces_[r.pos] = r.furnace;
  WorldSave::loadGuests(dir, guest_records);
  return true;
}

bool Game::save() {
  if (save_dir.empty()) return false;

  WorldSave::LevelData level;
  level.seed         = g_settings.world_seed;
  level.noise_offset = g_settings.noise_offset;
  level.worldgen     = worldgen_;
  level.time_of_day  = clock_.time_of_day;
  level.mode         = mode_;

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
  std::optional<std::vector<ItemId>> discovered = saved_discovered;
  if (host) discovered = seenList(host->seen);

  // Guests in the game now, then everyone who has ever played here.
  for (const Client &c : clients)
    if (c.player && c.role == Role::Guest) storeGuest(players.at(*c.player));
  bool guests_ok = true;
  for (const auto &[name, rec] : guest_records)
    guests_ok &= WorldSave::saveGuest(save_dir, name, rec);

  std::vector<WorldSave::FurnaceRecord> furnaces;
  for (const auto &[pos, f] : furnaces_) furnaces.push_back({pos, f});

  if (!WorldSave::save(save_dir, level, pd, world_.snapshotEdits()) ||
      !WorldSave::saveFurnaces(save_dir, furnaces) ||
      (inv && !WorldSave::saveInventory(save_dir, *inv)) ||
      (discovered && !WorldSave::saveDiscovered(save_dir, *discovered)) || !guests_ok) {
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
  mode_ = level.mode;
  world_.setLoadedEdits(std::move(edits));
  saved_player = std::move(player);
  saved_inventory = std::move(inventory);
}

void Game::addClient(ClientLink &link, Role role) { clients.push_back({&link, {}, role}); }

void Game::removeClient(ClientLink &link) {
  auto it = std::find_if(clients.begin(), clients.end(),
                         [&](const Client &c) { return c.link == &link; });
  if (it == clients.end()) return;
  std::optional<PlayerId> gone = it->player;
  bool guest = it->role == Role::Guest;
  clients.erase(it);
  if (!gone) return;
  // A guest's stuff is written straight away, so a host crash can't lose it.
  if (guest) {
    const PlayerState &ps = players.at(*gone);
    storeGuest(ps);
    if (!save_dir.empty() &&
        !WorldSave::saveGuest(save_dir, ps.name, guest_records.at(ps.name)))
      std::cerr << "[game] couldn't save guest '" << ps.name << "'\n";
  }
  players.erase(*gone);
  broadcast(PlayerLeft{*gone});
}

const WorldSave::GuestRecord *Game::guestRecord(const std::string &name) const {
  auto it = guest_records.find(name);
  return it == guest_records.end() ? nullptr : &it->second;
}

void Game::storeGuest(const PlayerState &ps) {
  WorldSave::GuestRecord rec;
  rec.player = {ps.position, ps.yaw, ps.pitch, ps.selected_slot,
                static_cast<uint8_t>(ps.fly ? 1 : 0)};
  rec.inventory.assign(ps.inventory.slots().begin(), ps.inventory.slots().end());
  rec.discovered = seenList(ps.seen);
  guest_records[ps.name] = std::move(rec);
}

bool Game::validName(std::string_view name) {
  if (name.empty() || name.size() > kMaxNameLength) return false;
  return std::all_of(name.begin(), name.end(), [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
  });
}

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

// Debug-panel cheats belong to the host; a guest's attempt is refused.
bool Game::hostOnly(Client &client, std::string_view what) {
  if (client.role == Role::Host) return true;
  reject(client, std::string(what) + ": host only");
  return false;
}

void Game::broadcast(const Event &ev) {
  for (Client &client : clients) client.link->deliver(ev);
}

void Game::sendInventory(Client &client, PlayerState &ps) {
  // Every way an item arrives (pickups, crafting, the palette, the kit, a
  // loaded save) ends here, so this is where items count as seen.
  for (const ItemStack &s : ps.inventory.slots()) see(ps.seen, s.id);
  client.link->deliver(InventoryContents{ps.inventory.slots()});
  SeenItems fresh = ps.seen & ~ps.seen_sent;
  if (fresh.any()) {
    client.link->deliver(KnownItems{seenList(fresh)});
    ps.seen_sent |= fresh;
  }
}

Inventory Game::starterKit() {
  Inventory kit;
  // Glowstone is the only light source and never generates naturally.
  kit.set(0, {itemForBlock(BlockType::GLOWSTONE), 8});
  return kit;
}

bool Game::tableInReach(const PlayerState &ps) const {
  return craftingTableWithinReach(world_, ps.eyePosition());
}

bool Game::craftingTableWithinReach(const World &world, glm::vec3 eye) {
  constexpr float kHalfBlockDiagonal = 0.8660254f; // as in inReach()
  const glm::ivec3 at = glm::floor(eye);
  const int r = static_cast<int>(std::ceil(kPlayerReach + 1.0f));
  for (int dy = -r; dy <= r; ++dy)
    for (int dz = -r; dz <= r; ++dz)
      for (int dx = -r; dx <= r; ++dx) {
        glm::ivec3 p = at + glm::ivec3(dx, dy, dz);
        if (world.getBlockAt(glm::vec3(p) + 0.5f) == BlockType::CRAFTING_TABLE &&
            glm::distance(eye, glm::vec3(p) + 0.5f) <= kPlayerReach + kHalfBlockDiagonal)
          return true;
      }
  return false;
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
    ps->attack_cooldown_ms = std::max(0, ps->attack_cooldown_ms - kTickMs);
    if (ps->vitals.dead()) {
      ps->dead_ms += kTickMs;
      if (ps->dead_ms >= kRespawnDelayMs) respawn(client, *ps);
    } else if (rules().drain) {
      ps->vitals.tick(kTickMs, ps->activity);
    } else {
      ps->vitals.reset(); // creative: always full
    }
  }

  std::vector<glm::vec3> positions, living;
  for (const auto &[id, ps] : players) {
    positions.push_back(ps.position);
    if (!ps.vitals.dead() && rules().enemies_target) living.push_back(ps.position);
  }
  entities_.tick(world_, rng_, positions, living, speedScale(g_settings.difficulty));
  removeEnemies();
  enemyAttacks();
  spawner_.tick(entities_, world_, rng_, positions, clock_.fractionOfDay(),
                g_settings.difficulty);
  releaseAnimalProducts();
  tickFurnaces();
  mergeDroppedItems();
  pickUpDroppedItems();

  ++tick_count;
  flushVitals();
  sendEntitySnapshots();
  if (tick_count % kTimeSyncTicks == 0)
    for (Client &client : clients)
      if (client.player && client.role == Role::Guest)
        client.link->deliver(TimeSync{clock_.time_of_day, clock_.scale});
}

void Game::sendEntitySnapshots() {
  for (Client &client : clients) {
    PlayerState *ps = playerFor(client);
    if (!ps) continue;
    EntitySnapshot snap;
    snap.tick = tick_count;
    for (const auto &[id, e] : entities_.all()) {
      if (glm::distance(e.body.position, ps->position) > kEntityViewRange) continue;
      snap.entities.push_back({id, e.kind, e.body.position, e.yaw, e.item.id,
                               e.hurt_ms > 0, e.happy_ms > 0, false});
    }
    for (const auto &[pid, other] : players) {
      if (pid == ps->id || other.vitals.dead()) continue;
      if (glm::distance(other.position, ps->position) > kEntityViewRange) continue;
      snap.players.push_back({pid, other.name, other.position, other.yaw, other.pitch,
                              other.crouch, false});
    }
    client.link->deliver(std::move(snap));
  }
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
  ++ps.respawn;
  client.link->deliver(Respawned{ps.position, ps.respawn});
}

void Game::rescueFromVoid(Client &client, PlayerState &ps) {
  glm::vec3 to = respawnPoint();
  const int x = static_cast<int>(std::floor(ps.position.x));
  const int z = static_cast<int>(std::floor(ps.position.z));
  for (int y = kChunkHeight - 1; y >= 0; --y)
    if (world_.getBlockAt(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f)) != BlockType::AIR) {
      to = glm::vec3(x + 0.5f, y + 1.0f, z + 0.5f);
      break;
    }
  ps.fall.reset();
  ps.position = to;
  ++ps.respawn; // moves sent from down there are stale now
  client.link->deliver(Respawned{ps.position, ps.respawn});
}

// ─── Furnaces ──────────────────────────────────────────────────────────

const Furnace *Game::furnaceAt(glm::ivec3 pos) const {
  auto it = furnaces_.find(pos);
  return it == furnaces_.end() ? nullptr : &it->second;
}

void Game::tickFurnaces() {
  // Viewers see slot changes at once and the gauges move four times a second.
  const bool gauges = tick_count % (250 / kTickMs) == 0;
  for (auto &[pos, furnace] : furnaces_) {
    FurnaceStep step = tickFurnace(furnace, kTickMs);
    if (step.lit_changed) showFurnaceLit(pos, furnace.lit());
    if (step.slots_changed || step.lit_changed || (gauges && furnace.lit()))
      sendContainerToViewers(pos);
  }
  // Walked away, or the furnace is gone: it closes for them.
  for (Client &c : clients)
    if (PlayerState *ps = playerFor(c); ps && ps->open_container && !openFurnace(*ps))
      closeContainer(c, *ps);
}

void Game::showFurnaceLit(glm::ivec3 pos, bool lit) {
  BlockType look = lit ? BlockType::FURNACE_LIT : BlockType::FURNACE;
  world_.applyEdit(pos, look); // recorded even if the chunk isn't loaded
  broadcast(BlockChanged{pos, look});
}

void Game::spillFurnace(glm::ivec3 pos) {
  auto it = furnaces_.find(pos);
  if (it == furnaces_.end()) return;
  std::uniform_real_distribution<float> side(-1.0f, 1.0f);
  for (const ItemStack &s : {it->second.input, it->second.fuel, it->second.output})
    if (!s.empty())
      dropItem(s, glm::vec3(pos) + glm::vec3(0.5f, 0.3f, 0.5f), {side(rng_), 4.0f, side(rng_)},
               kBreakDropDelayMs);
  furnaces_.erase(it);
}

// ─── Commands ──────────────────────────────────────────────────────────

// Every Command alternative needs an apply() overload; a missing one is a
// compile error here rather than a silently dropped command.
void Game::apply(Client &client, const Command &cmd) {
  std::visit([&](const auto &c) { apply(client, c); }, cmd);
}

void Game::apply(Client &client, const Join &cmd) {
  if (client.player) return; // already joined

  PlayerState ps;
  ps.id = next_player_id;
  ps.name = cmd.name.empty() ? "Player" + std::to_string(ps.id) : cmd.name;
  if (!validName(ps.name))
    return client.link->deliver(JoinRefused{"Names are 1-16 letters, digits, _ or -"});
  for (const auto &[id, other] : players)
    if (other.name == ps.name) return client.link->deliver(JoinRefused{"Name already in use"});
  if (players.size() >= kMaxPlayers) return client.link->deliver(JoinRefused{"Game is full"});
  ++next_player_id;
  const WorldSave::GuestRecord *rec =
      client.role == Role::Guest ? guestRecord(ps.name) : nullptr;
  if (rec) {
    ps.position = rec->player.position;
    ps.yaw = rec->player.yaw;
    ps.pitch = rec->player.pitch;
    ps.selected_slot = rec->player.selected_slot;
    ps.fly = rec->player.fly_mode != 0;
  } else if (client.role == Role::Host && saved_player) {
    ps.position = saved_player->position;
    ps.yaw = saved_player->yaw;
    ps.pitch = saved_player->pitch;
    ps.selected_slot = saved_player->selected_slot;
    ps.fly = saved_player->fly_mode != 0;
    saved_player.reset();
  } else {
    ps.position = world_.findSpawn();
  }
  // A guest gets their record's inventory and the host takes over the saved
  // one, like the saved player; anyone without one starts with the kit.
  if (rec) {
    for (int i = 0; i < kInventorySlots && i < static_cast<int>(rec->inventory.size()); ++i)
      ps.inventory.set(i, rec->inventory[i]);
    for (ItemId id : rec->discovered) see(ps.seen, id);
  } else if (client.role == Role::Host && saved_inventory) {
    for (int i = 0; i < kInventorySlots && i < static_cast<int>(saved_inventory->size()); ++i)
      ps.inventory.set(i, (*saved_inventory)[i]);
    saved_inventory.reset();
  } else {
    ps.inventory = starterKit();
  }

  if (client.role == Role::Host && saved_discovered) {
    for (ItemId id : *saved_discovered) see(ps.seen, id);
    saved_discovered.reset();
  }

  // A guest builds its own copy of the world before it hears it has joined.
  if (client.role == Role::Guest)
    client.link->deliver(Welcome{seed_, noise_offset_,
                                 clock_.time_of_day, g_settings.time_scale,
                                 world_.editList(), worldgen_});
  for (Client &other : clients)
    if (other.player) other.link->deliver(PlayerJoined{ps.id, ps.name});
  PlayerState &joined = players[ps.id] = std::move(ps);
  client.player = joined.id;
  // Flying from a creative visit doesn't carry into survival.
  if (!rules().fly) joined.fly = false;
  client.link->deliver(Joined{joined.id, joined.position, joined.yaw, joined.pitch,
                              joined.selected_slot, joined.fly, mode_});
  sendInventory(client, joined);
}

void Game::apply(Client &client, const MovePlayer &cmd) {
  PlayerState *ps = playerFor(client);
  // The dead stay put until they respawn, and moves from before the respawn
  // (still in flight when it happened) are stale.
  if (!ps || ps->vitals.dead() || cmd.respawn != ps->respawn) return;
  if (cmd.position.y < kVoidY) {
    ps->position = cmd.position;
    return rescueFromVoid(client, *ps);
  }
  ps->position = cmd.position;
  ps->yaw = cmd.yaw;
  ps->pitch = cmd.pitch;
  ps->fly = cmd.fly;
  ps->crouch = cmd.crouch;
  ps->activity = {cmd.sprinting, cmd.eyes_underwater, cmd.fly};

  int fall = ps->fall.update(cmd.position.y, cmd.on_ground, cmd.in_water, cmd.fly);
  if (fall > 0 && rules().damage) ps->vitals.damage(fall);
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

  // Survival: it takes as long as the block and the tool in hand say, from
  // the StartBreak on this block (judged by what's held now).
  const ItemId held = ps->inventory.slot(ps->selected_slot).id;
  if (rules().timed_mining) {
    int need = breakMs(target, held);
    bool started = ps->breaking && ps->breaking->pos == cmd.pos;
    if (!started || nowMs() - ps->breaking->since_ms + kBreakSlackMs < static_cast<uint64_t>(need))
      return reject(client, "break: not yet");
    ps->breaking.reset();
  }

  world_.setBlockAt(cmd.pos, BlockType::AIR);
  broadcast(BlockChanged{cmd.pos, BlockType::AIR});
  spillFurnace(cmd.pos);

  // Survival: collect the drop, if the tool was good enough for it.
  if (!rules().block_drops) return;
  if (auto drop = dropFor(target); drop && dropsWith(target, held)) {
    ItemStack got{*drop, 1};
    // Emerald pickaxe: a one in three chance of a second from ore.
    if (held == ItemId::EMERALD_PICKAXE && isOre(target) &&
        std::uniform_int_distribution<int>(0, 2)(rng_) == 0)
      got.count = 2;
    ItemStack left = ps->inventory.add(got);
    // No room: it pops out of the block instead.
    if (!left.empty()) {
      std::uniform_real_distribution<float> side(-1.0f, 1.0f);
      dropItem(left, glm::vec3(cmd.pos) + glm::vec3(0.5f, 0.3f, 0.5f),
               {side(rng_), 4.0f, side(rng_)}, kBreakDropDelayMs);
    }
  }
  if (rules().timed_mining) wearHeldTool(client, *ps);
  sendInventory(client, *ps);
}

void Game::apply(Client &client, const StartBreak &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  ps->breaking = PlayerState::Breaking{cmd.pos, nowMs()};
}

void Game::wearHeldTool(Client &client, PlayerState &ps) {
  ItemStack held = ps.inventory.slot(ps.selected_slot);
  Tool tool = toolOf(held.id);
  if (tool.kind == ToolKind::None) return;
  if (++held.wear >= maxUses(tool.tier)) {
    ps.inventory.set(ps.selected_slot, {});
    client.link->deliver(ToolBroke{held.id});
  } else {
    ps.inventory.set(ps.selected_slot, held);
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
  if (*block == BlockType::FURNACE) furnaces_[cmd.pos] = Furnace{};
  if (rules().consume_blocks) {
    ps->inventory.take(ps->selected_slot, 1);
    sendInventory(client, *ps);
  }
}

void Game::apply(Client &client, const SetTimeOfDay &cmd) {
  if (!hostOnly(client, "time")) return;
  clock_.time_of_day = std::fmod(std::max(cmd.seconds, 0.0f), 86400.0f);
}

void Game::apply(Client &client, const Save &) {
  if (hostOnly(client, "save")) save();
}

void Game::apply(Client &client, const GiveItem &cmd) {
  PlayerState *ps = playerFor(client);
  // The creative palette is for everyone; otherwise it's a host cheat.
  if (!ps || (!rules().palette && !hostOnly(client, "give"))) return;
  if (!isValidItem(cmd.id) || cmd.count == 0)
    return reject(client, "give: unknown item");
  // Same as collecting a drop: whatever doesn't fit lands at your feet.
  ItemStack left = ps->inventory.add({cmd.id, cmd.count});
  sendInventory(client, *ps);
  while (!left.empty()) {
    ItemStack stack = split(left, itemDef(left.id).max_stack);
    if (!dropItem(stack, ps->position + glm::vec3(0, 0.5f, 0), {0, 2.0f, 0},
                  kThrowDropDelayMs))
      break;
  }
}

namespace {

bool isContainerSlot(int slot) { return slot >= kFurnaceInput && slot <= kFurnaceOutput; }

// What a slot will take: anything in the inventory; smeltables in the input,
// fuel in the fuel slot, nothing put into the output.
bool accepts(int slot, ItemId id) {
  if (slot == kFurnaceInput) return smeltResult(id).has_value();
  if (slot == kFurnaceFuel) return fuelMs(id) > 0;
  if (slot == kFurnaceOutput) return false;
  return true;
}

ItemStack *furnaceSlot(Furnace &f, int slot) {
  if (slot == kFurnaceInput) return &f.input;
  if (slot == kFurnaceFuel) return &f.fuel;
  if (slot == kFurnaceOutput) return &f.output;
  return nullptr;
}

} // namespace

void Game::apply(Client &client, const MoveItems &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  if (!isContainerSlot(cmd.from) && !isContainerSlot(cmd.to)) {
    if (!ps->inventory.move(cmd.from, cmd.to, cmd.count))
      return reject(client, "move: not allowed");
    return sendInventory(client, *ps);
  }

  // To or from the open furnace.
  Furnace *f = openFurnace(*ps);
  if (!f) return reject(client, "move: no furnace open");
  auto valid = [](int s) { return Inventory::validSlot(s) || isContainerSlot(s); };
  if (!valid(cmd.from) || !valid(cmd.to) || cmd.from == cmd.to)
    return reject(client, "move: not allowed");
  ItemStack src = isContainerSlot(cmd.from) ? *furnaceSlot(*f, cmd.from)
                                            : ps->inventory.slot(cmd.from);
  ItemStack dst = isContainerSlot(cmd.to) ? *furnaceSlot(*f, cmd.to)
                                          : ps->inventory.slot(cmd.to);
  bool swapping = !dst.empty() && dst.id != src.id;
  if (src.empty() || !accepts(cmd.to, src.id) || (swapping && !accepts(cmd.from, dst.id)) ||
      !moveStack(src, dst, cmd.count))
    return reject(client, "move: not allowed");

  auto store = [&](int slot, const ItemStack &s) {
    if (isContainerSlot(slot)) *furnaceSlot(*f, slot) = s;
    else ps->inventory.set(slot, s);
  };
  store(cmd.from, src);
  store(cmd.to, dst);
  sendInventory(client, *ps);
  sendContainerToViewers(*ps->open_container);
}

void Game::apply(Client &client, const OpenContainer &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  if (!inReach(*ps, cmd.pos)) return reject(client, "open: out of reach");
  BlockType b = world_.getBlockAt(glm::vec3(cmd.pos) + 0.5f);
  if (b != BlockType::FURNACE && b != BlockType::FURNACE_LIT)
    return reject(client, "open: that isn't a furnace");
  furnaces_.try_emplace(cmd.pos); // a furnace block with no record yet starts empty
  ps->open_container = cmd.pos;
  sendContainer(client, cmd.pos);
}

void Game::apply(Client &client, const CloseContainer &) {
  if (PlayerState *ps = playerFor(client)) ps->open_container.reset();
}

Furnace *Game::openFurnace(const PlayerState &ps) {
  if (!ps.open_container || !inReach(ps, *ps.open_container)) return nullptr;
  return furnaceAt(*ps.open_container);
}

void Game::sendContainer(Client &client, glm::ivec3 pos) {
  ContainerView view;
  view.pos = pos;
  if (const Furnace *f = furnaceAt(pos)) {
    view.input = f->input;
    view.fuel = f->fuel;
    view.output = f->output;
    if (f->burn_total_ms > 0) view.burn = static_cast<uint8_t>(f->burn_ms * 255 / f->burn_total_ms);
    view.cook = static_cast<uint8_t>(f->cook_ms * 255 / kSmeltMs);
  } else {
    view.open = false;
  }
  client.link->deliver(view);
}

void Game::sendContainerToViewers(glm::ivec3 pos) {
  for (Client &c : clients)
    if (PlayerState *ps = playerFor(c); ps && ps->open_container == pos) sendContainer(c, pos);
}

void Game::closeContainer(Client &client, PlayerState &ps) {
  ContainerView closed;
  closed.pos = *ps.open_container;
  closed.open = false;
  ps.open_container.reset();
  client.link->deliver(closed);
}

// ─── Dropped items ─────────────────────────────────────────────────────

void Game::apply(Client &client, const DropItem &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  const ItemStack &held = ps->inventory.slot(ps->selected_slot);
  if (held.empty()) return reject(client, "drop: nothing held");

  ItemStack thrown = ps->inventory.take(ps->selected_slot, cmd.whole_stack ? held.count : 1);
  sendInventory(client, *ps);

  // Throw along the look direction (same convention as the camera).
  float yaw = glm::radians(ps->yaw), pitch = glm::radians(ps->pitch);
  glm::vec3 look{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                 std::sin(yaw) * std::cos(pitch)};
  glm::vec3 from = ps->eyePosition() - glm::vec3(0.0f, 0.3f, 0.0f);
  dropItem(thrown, from, look * 6.0f + glm::vec3(0.0f, 2.0f, 0.0f), kThrowDropDelayMs);
}

bool Game::dropItem(ItemStack stack, glm::vec3 pos, glm::vec3 velocity,
                    int pickup_delay_ms) {
  if (stack.empty()) return true;
  auto id = entities_.spawn(EntityKind::DroppedItem, pos);
  if (!id) {
    std::cerr << "[game] entity cap reached; dropped item lost\n";
    return false;
  }
  Entity *e = entities_.find(*id);
  e->item = stack;
  e->body.velocity = velocity;
  e->pickup_delay_ms = pickup_delay_ms;
  return true;
}

// Identical items lying close together become one stack (the older entity
// keeps it), so a pile of drops stays one pickup and one draw.
void Game::mergeDroppedItems() {
  auto &all = entities_.all();
  std::vector<EntityId> merged;
  for (auto a = all.begin(); a != all.end(); ++a) {
    Entity &ea = a->second;
    if (ea.kind != EntityKind::DroppedItem || ea.item.empty()) continue;
    for (auto b = std::next(a); b != all.end(); ++b) {
      Entity &eb = b->second;
      if (eb.kind != EntityKind::DroppedItem || eb.item.empty()) continue;
      if (eb.item.id != ea.item.id) continue;
      if (glm::distance(ea.body.position, eb.body.position) > kMergeRange) continue;
      mergeInto(ea.item, eb.item);
      ea.pickup_delay_ms = std::max(ea.pickup_delay_ms, eb.pickup_delay_ms);
      if (eb.item.empty()) merged.push_back(b->first);
    }
  }
  for (EntityId id : merged) entities_.remove(id);
}

void Game::pickUpDroppedItems() {
  std::vector<EntityId> taken;
  for (Client &client : clients) {
    PlayerState *ps = playerFor(client);
    if (!ps || ps->vitals.dead()) continue;
    glm::vec3 centre = ps->position + glm::vec3(0.0f, 0.9f, 0.0f);
    bool changed = false;
    for (auto &[id, e] : entities_.all()) {
      if (e.kind != EntityKind::DroppedItem || e.item.empty()) continue;
      if (e.pickup_delay_ms > 0) continue;
      if (glm::distance(e.body.position, centre) > kPickupRange) continue;
      ItemStack before = e.item;
      e.item = ps->inventory.add(e.item);
      if (e.item == before) continue; // no room at all
      changed = true;
      if (e.item.empty()) taken.push_back(id);
    }
    if (changed) sendInventory(client, *ps);
  }
  for (EntityId id : taken) entities_.remove(id);
}

// ─── Hitting entities ──────────────────────────────────────────────────

void Game::apply(Client &client, const AttackEntity &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  Entity *e = entities_.find(cmd.id);
  if (!e) return reject(client, "attack: no such entity");
  const EntityTypeDef &def = entityType(e->kind);
  if (def.max_health <= 0) return reject(client, "attack: can't hit that");
  if (ps->attack_cooldown_ms > 0) return reject(client, "attack: too soon");

  if (!inReachOf(*ps, *e)) return reject(client, "attack: out of reach");

  ps->attack_cooldown_ms = kAttackCooldownMs;
  e->health -= kAttackDamage;
  e->hurt_ms = kHurtFlashMs;

  // Knock it away from the attacker (straight back if they overlap).
  glm::vec2 away(e->body.position.x - ps->position.x, e->body.position.z - ps->position.z);
  if (glm::length(away) < 1e-3f) {
    float yaw = glm::radians(ps->yaw);
    away = {std::cos(yaw), std::sin(yaw)};
  }
  away = glm::normalize(away) * kKnockback;
  e->body.velocity = {away.x, kKnockUp, away.y};
  e->body.on_ground = false;

  if (e->health <= 0) {
    std::uniform_real_distribution<float> side(-1.0f, 1.0f);
    glm::vec3 at = e->body.position + glm::vec3(0.0f, 0.3f, 0.0f);
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    for (const ItemStack &drop : def.drops)
      dropItem(drop, at, {side(rng_), 4.0f, side(rng_)}, kBreakDropDelayMs);
    for (const ChanceDrop &drop : def.chance_drops)
      if (roll(rng_) < drop.chance)
        dropItem(drop.stack, at, {side(rng_), 4.0f, side(rng_)}, kBreakDropDelayMs);
    entities_.remove(cmd.id);
  }
}

void Game::apply(Client &client, const SpawnDebugEntity &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead() || !hostOnly(client, "spawn")) return;
  if (!isValidEntityKind(cmd.kind)) return reject(client, "spawn: unknown kind");
  float yaw = glm::radians(ps->yaw);
  glm::vec3 ahead{std::cos(yaw), 0.0f, std::sin(yaw)};
  if (!entities_.spawn(cmd.kind, ps->position + ahead * 3.0f + glm::vec3(0, 0.5f, 0)))
    reject(client, "spawn: entity cap reached");
}

// Reach is measured to the nearest point of the entity's box, matching the
// client's ray against that box.
bool Game::inReachOf(const PlayerState &ps, const Entity &e) const {
  glm::vec3 lo = e.body.position - glm::vec3(e.body.half_width, 0, e.body.half_width);
  glm::vec3 hi = e.body.position + glm::vec3(e.body.half_width, e.body.height,
                                             e.body.half_width);
  glm::vec3 eye = ps.eyePosition();
  return glm::distance(eye, glm::clamp(eye, lo, hi)) <= kPlayerReach;
}

// ─── Animals and food ──────────────────────────────────────────────────

void Game::apply(Client &client, const InteractEntity &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  Entity *e = entities_.find(cmd.id);
  if (!e || !isAnimal(e->kind)) return reject(client, "interact: nothing to do");
  if (!inReachOf(*ps, *e)) return reject(client, "interact: out of reach");

  const AnimalDef &animal = *entityType(e->kind).animal;
  const ItemStack &held = ps->inventory.slot(ps->selected_slot);
  if (held.empty() || held.id != animal.favourite)
    return reject(client, "feed: it doesn't want that");
  if (e->fed_cooldown_ms > 0) return reject(client, "feed: not hungry yet");

  ps->inventory.take(ps->selected_slot, 1);
  sendInventory(client, *ps);
  e->happy_ms = kHappyMs;
  e->fed_cooldown_ms = kFedCooldownMs;
  e->product_delay_ms = kProductDelayMs;
  e->product_pending = true;
}

// A fed animal's product pops out once its short delay has passed.
void Game::releaseAnimalProducts() {
  std::vector<std::pair<ItemStack, glm::vec3>> out;
  for (auto &[id, e] : entities_.all()) {
    if (!e.product_pending || e.product_delay_ms > 0) continue;
    e.product_pending = false;
    out.push_back({entityType(e.kind).animal->product,
                   e.body.position + glm::vec3(0.0f, e.body.height * 0.6f, 0.0f)});
  }
  // Spawn after iterating: dropItem adds to the store being walked.
  std::uniform_real_distribution<float> side(-1.0f, 1.0f);
  for (auto &[stack, at] : out)
    dropItem(stack, at, {side(rng_), 3.5f, side(rng_)}, kBreakDropDelayMs);
}

void Game::apply(Client &client, const Craft &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  if (cmd.recipe >= recipes().size()) return reject(client, "craft: no such recipe");
  const Recipe &recipe = recipes()[cmd.recipe];
  if (mode_ != GameMode::Creative && !knows(recipe, ps->seen))
    return reject(client, "craft: you haven't discovered that yet");
  if (recipe.needs_table && !tableInReach(*ps))
    return reject(client, "craft: needs a Crafting Table nearby");
  if (timesAffordable(recipe, ps->inventory) < 1)
    return reject(client, "craft: missing ingredients");

  int made = 0;
  while (auto after = craftOnce(recipe, ps->inventory)) {
    ps->inventory = *after;
    ++made;
    if (!cmd.all) break;
  }
  if (made == 0) return reject(client, "craft: no room in your inventory");
  sendInventory(client, *ps);
}

void Game::apply(Client &client, const EatItem &) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  const ItemStack &held = ps->inventory.slot(ps->selected_slot);
  if (held.empty() || itemDef(held.id).food_heal <= 0)
    return reject(client, "eat: that isn't food");
  if (ps->vitals.health() >= Vitals::kMax) return reject(client, "eat: not hungry");

  int heal = itemDef(held.id).food_heal;
  ps->inventory.take(ps->selected_slot, 1);
  sendInventory(client, *ps);
  ps->vitals.heal(heal);
}

// ─── Enemies ───────────────────────────────────────────────────────────

void Game::removeEnemies() {
  bool peaceful = g_settings.difficulty == Difficulty::Peaceful;
  bool day = !isNight(clock_.fractionOfDay());
  std::vector<EntityId> gone;
  for (const auto &[id, e] : entities_.all()) {
    if (!isEnemy(e.kind)) continue;
    if (peaceful) {
      gone.push_back(id);
      continue;
    }
    // Sunrise: surface enemies out under the sky simply vanish (no drops).
    const glm::vec3 &p = e.body.position;
    glm::ivec3 feet(std::floor(p.x), std::floor(p.y + 0.01f), std::floor(p.z));
    if (day && entityType(e.kind).enemy->vanish_at_dawn &&
        world_.getSkyLightAt(feet) >= kSurfaceEnemyMinSkyLight)
      gone.push_back(id);
  }
  for (EntityId id : gone) entities_.remove(id);
}

namespace {

// Distance between two boxes (0 when they touch or overlap).
float boxGap(glm::vec3 a_lo, glm::vec3 a_hi, glm::vec3 b_lo, glm::vec3 b_hi) {
  glm::vec3 gap = glm::max(glm::vec3(0.0f), glm::max(a_lo - b_hi, b_lo - a_hi));
  return glm::length(gap);
}

} // namespace

// An enemy touching a living player hurts them and knocks them away, then
// waits out its own cooldown.
void Game::enemyAttacks() {
  if (!rules().enemies_target) return;
  for (Client &client : clients) {
    PlayerState *ps = playerFor(client);
    if (!ps || ps->vitals.dead()) continue;
    glm::vec3 p_lo = ps->position - glm::vec3(kPlayerHalfWidth, 0.0f, kPlayerHalfWidth);
    glm::vec3 p_hi = ps->position + glm::vec3(kPlayerHalfWidth, ps->height(), kPlayerHalfWidth);

    for (auto &[id, e] : entities_.all()) {
      if (!isEnemy(e.kind) || e.attack_cooldown_ms > 0) continue;
      glm::vec3 half(e.body.half_width, 0.0f, e.body.half_width);
      glm::vec3 e_lo = e.body.position - half;
      glm::vec3 e_hi = e.body.position + half + glm::vec3(0.0f, e.body.height, 0.0f);
      if (boxGap(p_lo, p_hi, e_lo, e_hi) > kEnemyContactRange) continue;

      ps->vitals.damage(scaledDamage(entityType(e.kind).enemy->damage, g_settings.difficulty));
      e.attack_cooldown_ms = kEnemyAttackCooldownMs;

      glm::vec2 away(ps->position.x - e.body.position.x, ps->position.z - e.body.position.z);
      if (glm::length(away) < 1e-3f) {
        float yaw = glm::radians(e.yaw); // straight ahead of the enemy
        away = {std::sin(yaw), -std::cos(yaw)};
      }
      away = glm::normalize(away) * kEnemyKnockback;
      client.link->deliver(KnockedBack{{away.x, kEnemyKnockUp, away.y}});
      if (ps->vitals.dead()) break;
    }
  }
}
