#include "sim/game.hpp"
#include "block/block_data.hpp"
#include "block/door.hpp"
#include "item/farming.hpp"
#include "item/mining.hpp"
#include "item/tools.hpp"
#include "entity/animals.hpp"
#include "entity/boat.hpp"
#include "entity/enemies.hpp"
#include "entity/fish.hpp"
#include "core/settings.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iostream>

namespace {

// The files of a saved world, other than its guests.
constexpr const char *kWorldFiles[] = {"level.dat",     "player.dat",     "edits.dat",
                                       "inventory.dat", "discovered.dat", "furnaces.dat",
                                       "chests.dat",    "farms.dat",      "boats.dat"};

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
  registerFishSpawns(spawner_);
}

bool Game::load(const std::string &dir) {
  save_dir = dir;
  seed_ = g_settings.world_seed;
  noise_offset_ = g_settings.noise_offset;
  worldgen_ = g_settings.worldgen;

  // A file that is there but won't load is set aside, so the next save
  // doesn't replace it with less. Without level.dat the rest can't be used.
  auto setAside = [&](const char *file) { WorldSave::setAside(dir, file); };
  WorldSave::LevelData level;
  if (!WorldSave::loadLevel(dir, level)) {
    if (std::filesystem::exists(std::filesystem::path(dir) / "level.dat"))
      for (const char *file : kWorldFiles) setAside(file);
    return false;
  }

  g_settings.world_seed   = level.seed;
  g_settings.noise_offset = level.noise_offset;
  g_settings.worldgen     = level.worldgen;
  seed_ = level.seed;
  noise_offset_ = level.noise_offset;
  worldgen_ = level.worldgen;

  // A cut-short edits.dat still gives what it has: better than none.
  WorldEdits edits;
  if (!WorldSave::loadEdits(dir, edits)) setAside("edits.dat");
  std::optional<WorldSave::PlayerData> player;
  if (WorldSave::PlayerData pd; WorldSave::loadPlayer(dir, pd)) player = pd;
  else setAside("player.dat");
  std::optional<std::vector<ItemStack>> inventory;
  if (std::vector<ItemStack> inv; WorldSave::loadInventory(dir, inv))
    inventory = std::move(inv);
  else setAside("inventory.dat");

  restore(level, std::move(edits), player, std::move(inventory));
  if (std::vector<ItemId> ids; WorldSave::loadDiscovered(dir, ids))
    saved_discovered = std::move(ids);
  else setAside("discovered.dat");
  furnaces_.clear();
  if (std::vector<WorldSave::FurnaceRecord> fs; WorldSave::loadFurnaces(dir, fs))
    for (const auto &r : fs) furnaces_[r.pos] = r.furnace;
  else setAside("furnaces.dat");
  chests_.clear();
  if (std::vector<WorldSave::ChestRecord> cs; WorldSave::loadChests(dir, cs))
    for (const auto &r : cs) chests_[r.pos] = r.chest;
  else setAside("chests.dat");
  crops_.clear();
  soil_.clear();
  if (WorldSave::Farms farms; WorldSave::loadFarms(dir, farms)) {
    for (const auto &r : farms.crops) crops_[r.pos] = {r.left_ms};
    for (const auto &r : farms.soil) soil_[r.pos] = {r.wet_ms, r.by_water};
  } else {
    setAside("farms.dat");
  }
  // Without boats.dat (a world from before boats) there are none.
  saved_seats.clear();
  if (std::vector<WorldSave::BoatRecord> bs; WorldSave::loadBoats(dir, bs)) {
    for (const auto &r : bs)
      if (auto id = entities_.spawn(EntityKind::Boat, r.position)) {
        entities_.find(*id)->yaw = r.yaw;
        if (r.seats != SavedSeats{}) saved_seats[*id] = r.seats;
      }
  } else {
    setAside("boats.dat");
  }
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
    pd.position      = standingPosition(*host); // a rider stands beside their boat
    pd.yaw           = host->yaw;
    pd.pitch         = host->pitch;
    pd.selected_slot = host->selected_slot;
    pd.fly_mode      = host->fly ? 1 : 0;
    pd.bed           = host->bed;
    pd.pins          = host->pins;
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
  std::vector<WorldSave::ChestRecord> chests;
  for (const auto &[pos, c] : chests_) chests.push_back({pos, c});
  WorldSave::Farms farms;
  for (const auto &[pos, c] : crops_) farms.crops.push_back({pos, c.left_ms});
  for (const auto &[pos, s] : soil_) farms.soil.push_back({pos, s.wet_ms, s.by_water});
  // Each boat with who's in it: riders here now, else ones still to come back.
  std::vector<WorldSave::BoatRecord> boats;
  for (const auto &[id, e] : entities_.all()) {
    if (e.kind != EntityKind::Boat) continue;
    WorldSave::BoatRecord r{e.body.position, e.yaw};
    if (auto it = saved_seats.find(id); it != saved_seats.end()) r.seats = it->second;
    for (const auto &[pid, ps] : players) {
      if (!ps.riding || ps.riding->boat != id) continue;
      r.seats[ps.riding->seat] =
          &ps == host ? WorldSave::BoatRecord::Seat{WorldSave::BoatRecord::Rider::Host, ""}
                      : WorldSave::BoatRecord::Seat{WorldSave::BoatRecord::Rider::Guest, ps.name};
    }
    boats.push_back(r);
  }

  if (!WorldSave::save(save_dir, level, pd, world_.snapshotEdits()) ||
      !WorldSave::saveFurnaces(save_dir, furnaces) ||
      !WorldSave::saveChests(save_dir, chests) ||
      !WorldSave::saveFarms(save_dir, farms) ||
      !WorldSave::saveBoats(save_dir, boats) ||
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
  endLine(players.at(*gone));
  // A guest's stuff is written straight away, so a host crash can't lose it.
  if (guest) {
    const PlayerState &ps = players.at(*gone);
    storeGuest(ps);
    if (!save_dir.empty() &&
        !WorldSave::saveGuest(save_dir, ps.name, guest_records.at(ps.name)))
      std::cerr << "[game] couldn't save guest '" << ps.name << "'\n";
  }
  std::string name = players.at(*gone).name;
  players.erase(*gone);
  broadcast(PlayerLeft{*gone});
  postChat(0, "", name + " left the game");
}

const WorldSave::GuestRecord *Game::guestRecord(const std::string &name) const {
  auto it = guest_records.find(name);
  return it == guest_records.end() ? nullptr : &it->second;
}

void Game::storeGuest(const PlayerState &ps) {
  WorldSave::GuestRecord rec;
  rec.player = {standingPosition(ps), ps.yaw, ps.pitch, ps.selected_slot,
                static_cast<uint8_t>(ps.fly ? 1 : 0), ps.bed, ps.pins};
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

// ─── Chat ──────────────────────────────────────────────────────────────

uint16_t Game::localMinute() {
  std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  return static_cast<uint16_t>(local.tm_hour * 60 + local.tm_min);
}

void Game::postChat(PlayerId from, const std::string &name, const std::string &text) {
  ChatMessage msg{chat_minute(), from, name, text, false};
  chat_history.push_back(msg);
  while (chat_history.size() > kChatHistory) chat_history.pop_front();
  for (Client &client : clients)
    if (client.player) client.link->deliver(msg);
}

void Game::apply(Client &client, const Chat &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  std::optional<std::string> text = cleanChatText(cmd.text);
  if (!text) return;
  switch (ps->chat_limit.check(*text, static_cast<int64_t>(nowMs()))) {
    case ChatLimiter::Verdict::TooFast: return client.link->deliver(Notice{"Slow down!"});
    case ChatLimiter::Verdict::Repeat:  return;
    case ChatLimiter::Verdict::Ok:      break;
  }
  postChat(ps->id, ps->name, *text);
}

void Game::announceDeaths() {
  std::vector<std::string> lines;
  for (auto &[id, ps] : players) {
    if (!ps.vitals.dead()) {
      ps.death_told = false;
      continue;
    }
    if (ps.death_told) continue;
    ps.death_told = true;
    // Out of breath means the water got them, whatever hurt them before.
    std::string how = ps.vitals.breath() == 0 ? "drowned"
                      : ps.hurt_by.empty()    ? "died"
                                              : ps.hurt_by;
    lines.push_back(ps.name + " " + how);
  }
  for (const std::string &line : lines) postChat(0, "", line);
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
  flushSleep();
  flushWaypoints();
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
      ps->vitals.tick(kTickMs, ps->activity, needsFood(g_settings.difficulty));
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
  tickBoats();
  tickFishing();
  removeEnemies();
  enemyAttacks();
  spawner_.tick(entities_, world_, rng_, positions, clock_.fractionOfDay(),
                g_settings.difficulty);
  releaseAnimalProducts();
  burnEntities();
  tickFurnaces();
  if (tick_count % (1000 / kTickMs) == 0) growFarms(1000);
  mergeDroppedItems();
  pickUpDroppedItems();
  tickSleep();
  announceDeaths();

  ++tick_count;
  flushVitals();
  flushSleep();
  flushWaypoints();
  sendEntitySnapshots();
  if (tick_count % kTimeSyncTicks == 0)
    for (Client &client : clients)
      if (client.player && client.role == Role::Guest)
        client.link->deliver(TimeSync{clock_.time_of_day, clock_.scale});
  if (!save_dir.empty() && tick_count % kAutosaveTicks == 0) save();
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
                               e.hurt_ms > 0, e.happy_ms > 0, e.burning_ms > 0, e.owner,
                               e.biting, false});
    }
    for (const auto &[pid, other] : players) {
      if (pid == ps->id || other.vitals.dead()) continue;
      if (glm::distance(other.position, ps->position) > kEntityViewRange) continue;
      snap.players.push_back({pid, other.name, other.position, other.yaw, other.pitch,
                              other.crouch, other.sleeping_in.has_value(), false,
                              other.riding ? other.riding->boat : 0,
                              static_cast<uint8_t>(other.riding ? other.riding->seat : 0)});
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
    now.fed_s = (v.fedMs() + 999) / 1000;
    now.regen_blocked = rules().drain && needsFood(g_settings.difficulty) && !v.dead() &&
                        !v.wellFed() && v.health() < Vitals::kMax;
    if (ps->vitals_sent == now) continue;
    ps->vitals_sent = now;
    client.link->deliver(now);
  }
}

glm::vec3 Game::respawnPoint() {
  if (!respawn_point) respawn_point = world_.findSpawn();
  return *respawn_point;
}

// Back to their bed, or the world spawn, with full vitals. The inventory is
// kept.
void Game::respawn(Client &client, PlayerState &ps) {
  ps.vitals.reset();
  ps.fall.reset();
  ps.dead_ms = 0;
  if (ps.bed && !bedStillAt(*ps.bed)) {
    ps.bed.reset();
    client.link->deliver(Notice{"Your bed is missing"});
  }
  ps.position = ps.bed ? besideBed(*ps.bed) : respawnPoint();
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

// ─── Deaths and burning ────────────────────────────────────────────────

void Game::killEntity(EntityId id) {
  Entity *e = entities_.find(id);
  if (!e) return;
  const EntityTypeDef &def = entityType(e->kind);
  std::uniform_real_distribution<float> side(-1.0f, 1.0f);
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  glm::vec3 at = e->body.position + glm::vec3(0.0f, 0.3f, 0.0f);
  for (const ItemStack &drop : def.drops)
    dropItem(drop, at, {side(rng_), 4.0f, side(rng_)}, kBreakDropDelayMs);
  for (const ChanceDrop &drop : def.chance_drops)
    if (roll(rng_) < drop.chance)
      dropItem(drop.stack, at, {side(rng_), 4.0f, side(rng_)}, kBreakDropDelayMs);
  entities_.remove(id);
}

void Game::burnEntities() {
  std::vector<EntityId> dead;
  for (auto &[id, e] : entities_.all()) {
    if (e.burning_ms <= 0) continue;
    e.burning_ms -= kTickMs;
    e.burn_tick_ms += kTickMs;
    if (e.burn_tick_ms < 1000) continue;
    e.burn_tick_ms -= 1000;
    e.health -= 1;
    e.hurt_ms = kHurtFlashMs;
    if (e.health <= 0) dead.push_back(id);
  }
  for (EntityId id : dead) killEntity(id);
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
  // Walked away, or the furnace or chest is gone: it closes for them.
  for (Client &c : clients)
    if (PlayerState *ps = playerFor(c);
        ps && ps->open_container && !openFurnace(*ps) && !openChest(*ps))
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

// ─── Chests ────────────────────────────────────────────────────────────

const Chest *Game::chestAt(glm::ivec3 pos) const {
  auto it = chests_.find(pos);
  return it == chests_.end() ? nullptr : &it->second;
}

void Game::spillChest(glm::ivec3 pos) {
  auto it = chests_.find(pos);
  if (it == chests_.end()) return;
  std::uniform_real_distribution<float> side(-1.0f, 1.0f);
  for (const ItemStack &s : it->second)
    if (!s.empty())
      dropItem(s, glm::vec3(pos) + glm::vec3(0.5f, 0.3f, 0.5f), {side(rng_), 4.0f, side(rng_)},
               kBreakDropDelayMs);
  chests_.erase(it);
}

bool Game::unlockTreasure(Client &client, PlayerState &ps, glm::ivec3 pos) {
  if (ps.inventory.slot(ps.selected_slot).id != ItemId::TREASURE_KEY) return false;
  ps.inventory.take(ps.selected_slot, 1);
  sendInventory(client, ps);
  world_.setBlockAt(pos, BlockType::CHEST);
  broadcast(BlockChanged{pos, BlockType::CHEST});
  chests_[pos] = rollTreasure(rng_);
  return true;
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
  const bool saved_host = !rec && client.role == Role::Host && saved_player;
  if (rec) {
    ps.position = rec->player.position;
    ps.yaw = rec->player.yaw;
    ps.pitch = rec->player.pitch;
    ps.selected_slot = rec->player.selected_slot;
    ps.fly = rec->player.fly_mode != 0;
    ps.bed = rec->player.bed;
    ps.pins = rec->player.pins;
  } else if (client.role == Role::Host && saved_player) {
    ps.position = saved_player->position;
    ps.yaw = saved_player->yaw;
    ps.pitch = saved_player->pitch;
    ps.selected_slot = saved_player->selected_slot;
    ps.fly = saved_player->fly_mode != 0;
    ps.bed = saved_player->bed;
    ps.pins = saved_player->pins;
    saved_player.reset();
  } else {
    ps.position = respawnPoint();
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
  // A saved slot is only a hint: one off the hotbar (a damaged file) is the first.
  if (joined.selected_slot < 0 || joined.selected_slot >= kHotbarSlots) joined.selected_slot = 0;
  client.link->deliver(Joined{joined.id, joined.position, joined.yaw, joined.pitch,
                              joined.selected_slot, joined.fly, mode_});
  sendInventory(client, joined);
  retakeSeat(client, joined, saved_host);
  size_t skip = chat_history.size() > kChatOnJoin ? chat_history.size() - kChatOnJoin : 0;
  for (size_t i = skip; i < chat_history.size(); ++i) {
    ChatMessage old = chat_history[i];
    old.history = true;
    client.link->deliver(old);
  }
  postChat(0, "", joined.name + " joined the game");
}

void Game::apply(Client &client, const MovePlayer &cmd) {
  PlayerState *ps = playerFor(client);
  // The dead stay put until they respawn, and moves from before the respawn
  // (still in flight when it happened) are stale.
  if (!ps || ps->vitals.dead() || cmd.respawn != ps->respawn) return;
  // In bed, the Game holds them there; they get up with LeaveBed.
  if (ps->sleeping_in) return;
  // In a boat, they sit in their seat and only look around; the driver
  // also says where they steered the boat.
  if (ps->riding) {
    ps->yaw = cmd.yaw;
    ps->pitch = cmd.pitch;
    ps->crouch = false;
    ps->activity = {};
    ps->fall.reset();
    Entity *boat = entities_.find(ps->riding->boat);
    if (!boat) return;
    if (ps->riding->seat == 0 && cmd.boat == boat->id) {
      if (boatMoveAllowed(world_, boat->body.position, cmd.boat_position)) {
        boat->body.position = cmd.boat_position;
        boat->yaw = std::fmod(std::fmod(cmd.boat_yaw, 360.0f) + 360.0f, 360.0f);
      } else {
        // Back where the Game has it, so the two agree again.
        client.link->deliver(Riding{boat->id, 0, boat->body.position, boat->yaw});
      }
    }
    seatRiders(*boat);
    return;
  }
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
  if (fall > 0 && rules().damage) {
    ps->vitals.damage(fall);
    ps->hurt_by = "fell too far";
  }
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
  if (ps->sleeping_in) return reject(client, "break: in bed");
  if (ps->riding) return reject(client, "break: in a boat");
  if (cmd.pos.y < 0 || cmd.pos.y >= kChunkHeight)
    return reject(client, "break: outside world height");
  if (!inReach(*ps, cmd.pos)) return reject(client, "break: out of reach");

  BlockType target = world_.getBlockAt(glm::vec3(cmd.pos) + 0.5f);
  if (target == BlockType::AIR) return reject(client, "break: nothing there");
  if (target == BlockType::BEDROCK || target == BlockType::WATER ||
      target == BlockType::TREASURE_CHEST)
    return reject(client, "break: unbreakable block");

  // Survival: ore holds until there's a pickaxe good enough to get its drop.
  const ItemId held = ps->inventory.slot(ps->selected_slot).id;
  if (rules().block_drops && pickaxeNeeded(target, held) != ItemId::NONE)
    return reject(client, "break: needs a better pickaxe");

  // Survival: it takes as long as the block and the tool in hand say, from
  // the StartBreak on this block (judged by what's held now).
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
  spillChest(cmd.pos);
  // A door goes as a whole, whichever half was hit, and drops one door.
  if (isDoor(target)) {
    glm::ivec3 other = cmd.pos + glm::ivec3(0, doorTop(target) ? -1 : 1, 0);
    if (isDoor(world_.getBlockAt(glm::vec3(other) + 0.5f))) {
      world_.setBlockAt(other, BlockType::AIR);
      broadcast(BlockChanged{other, BlockType::AIR});
    }
    target = doorBlock(false, doorFacing(target), false);
  }
  // So does a bed, dropping one of its colour.
  if (isBed(target)) {
    glm::ivec3 other = bedOther(cmd.pos, target);
    if (isBed(world_.getBlockAt(glm::vec3(other) + 0.5f))) {
      world_.setBlockAt(other, BlockType::AIR);
      broadcast(BlockChanged{other, BlockType::AIR});
    }
    target = bedBlock(bedColour(target), false, bedFacing(target));
  }
  popDoorAbove(cmd.pos);
  popBedAbove(cmd.pos);
  popCropAbove(cmd.pos);
  popUnsupported(cmd.pos);
  crops_.erase(cmd.pos);
  soil_.erase(cmd.pos);

  // Survival: collect the drop, if the tool was good enough for it; a crop
  // gives its harvest.
  if (!rules().block_drops) return;
  if (isCrop(target)) {
    for (const ItemStack &got : harvest(target, rng_)) giveOrDrop(*ps, got, cmd.pos);
  } else if (auto drop = dropOf(target); drop && dropsWith(target, held)) {
    ItemStack got{*drop, 1};
    // Emerald pickaxe: a one in three chance of a second from ore.
    if (held == ItemId::EMERALD_PICKAXE && isOre(target) &&
        std::uniform_int_distribution<int>(0, 2)(rng_) == 0)
      got.count = 2;
    giveOrDrop(*ps, got, cmd.pos);
  }
  // What's picked at once (crops, plants, torches) doesn't wear the tool.
  if (rules().timed_mining && toughness(target).hand_ms > 0) wearHeldTool(client, *ps);
  sendInventory(client, *ps);
}

void Game::apply(Client &client, const StartBreak &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead() || ps->riding) return;
  ps->breaking = PlayerState::Breaking{cmd.pos, nowMs()};
}

bool Game::wearHeldTool(Client &client, PlayerState &ps) {
  if (toolOf(ps.inventory.slot(ps.selected_slot).id).kind == ToolKind::None) return false;
  return spendUse(client, ps);
}

bool Game::spendUse(Client &client, PlayerState &ps) {
  ItemStack held = ps.inventory.slot(ps.selected_slot);
  int uses = usesOf(held.id);
  if (uses <= 0) return false;
  if (++held.wear >= uses) {
    ps.inventory.set(ps.selected_slot, {});
    client.link->deliver(ToolBroke{held.id});
  } else {
    ps.inventory.set(ps.selected_slot, held);
  }
  return true;
}

void Game::apply(Client &client, const PlaceBlock &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  if (ps->sleeping_in) return reject(client, "place: in bed");
  if (ps->riding) return reject(client, "place: in a boat");

  const ItemStack &held = ps->inventory.slot(ps->selected_slot);
  if (held.empty()) return reject(client, "place: nothing held");
  if (held.id == ItemId::WOODEN_DOOR) return placeDoor(client, *ps, cmd.pos);
  if (isBedItem(held.id)) return placeBed(client, *ps, cmd.pos);
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
  // A plant, a torch or a cake goes on its own ground (a torch on a floor or
  // a wall), in the air.
  BlockType placed = *block;
  if (needsSupport(placed)) {
    if (isLiquid(existing)) return reject(client, "place: not in water");
    if (placed == BlockType::TORCH) {
      std::optional<BlockType> torch = torchFor(cmd.against);
      if (!torch) return reject(client, "place: torches go on floors and walls");
      placed = *torch;
    }
    const BlockType ground = world_.getBlockAt(glm::vec3(cmd.pos + supportOf(placed)) + 0.5f);
    if (!growsOn(placed, ground, isSolidGround(ground)))
      return reject(client, "place: it can't go there");
  }
  // What's walked through can go where someone stands.
  if (blocksMovement(placed) && overlapsAnyPlayer(cmd.pos))
    return reject(client, "place: inside a player");
  if (overlapsBoat(cmd.pos)) return reject(client, "place: a boat is there");

  world_.setBlockAt(cmd.pos, placed);
  broadcast(BlockChanged{cmd.pos, placed});
  packSoilUnder(cmd.pos);
  if (*block == BlockType::FURNACE) furnaces_[cmd.pos] = Furnace{};
  if (*block == BlockType::CHEST) chests_[cmd.pos] = Chest{};
  if (rules().consume_blocks) {
    ps->inventory.take(ps->selected_slot, 1);
    sendInventory(client, *ps);
  }
}

// ─── Doors ─────────────────────────────────────────────────────────────

void Game::setDoor(glm::ivec3 bottom, BlockType bottom_half) {
  const glm::ivec3 top = bottom + glm::ivec3(0, 1, 0);
  const BlockType top_half = doorBlock(true, doorFacing(bottom_half), doorOpen(bottom_half));
  world_.setBlockAt(bottom, bottom_half);
  world_.setBlockAt(top, top_half);
  broadcast(BlockChanged{bottom, bottom_half});
  broadcast(BlockChanged{top, top_half});
}

void Game::placeDoor(Client &client, PlayerState &ps, glm::ivec3 pos) {
  if (pos.y < 1 || pos.y + 1 >= kChunkHeight)
    return reject(client, "place: outside world height");
  if (!inReach(ps, pos)) return reject(client, "place: out of reach");
  int cx = static_cast<int>(std::floor(static_cast<float>(pos.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(pos.z) / kChunkDepth));
  if (!world_.getChunk(cx, cz)) return reject(client, "place: chunk not loaded");

  // Two clear cells, standing on something solid.
  const glm::ivec3 top = pos + glm::ivec3(0, 1, 0);
  for (glm::ivec3 cell : {pos, top}) {
    BlockType existing = world_.getBlockAt(glm::vec3(cell) + 0.5f);
    if (existing != BlockType::AIR && !isLiquid(existing))
      return reject(client, "place: no room for a door");
    if (overlapsAnyPlayer(cell)) return reject(client, "place: inside a player");
    if (overlapsBoat(cell)) return reject(client, "place: a boat is there");
  }
  BlockType below = world_.getBlockAt(glm::vec3(pos) + glm::vec3(0.5f, -0.5f, 0.5f));
  if (!blocksMovement(below) || isDoor(below) || isBed(below))
    return reject(client, "place: a door needs ground under it");

  setDoor(pos, doorBlock(false, doorFacingForYaw(ps.yaw), false));
  packSoilUnder(pos);
  if (rules().consume_blocks) {
    ps.inventory.take(ps.selected_slot, 1);
    sendInventory(client, ps);
  }
}

void Game::popDoorAbove(glm::ivec3 below) {
  const glm::ivec3 bottom = below + glm::ivec3(0, 1, 0);
  BlockType b = world_.getBlockAt(glm::vec3(bottom) + 0.5f);
  if (!isDoor(b) || doorTop(b)) return;
  for (glm::ivec3 cell : {bottom, bottom + glm::ivec3(0, 1, 0)}) {
    if (!isDoor(world_.getBlockAt(glm::vec3(cell) + 0.5f))) continue;
    world_.setBlockAt(cell, BlockType::AIR);
    broadcast(BlockChanged{cell, BlockType::AIR});
  }
  if (rules().block_drops)
    dropItem({ItemId::WOODEN_DOOR, 1}, glm::vec3(bottom) + glm::vec3(0.5f, 0.3f, 0.5f),
             {0.0f, 3.0f, 0.0f}, kBreakDropDelayMs);
}

void Game::apply(Client &client, const UseDoor &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  if (cmd.pos.y < 0 || cmd.pos.y >= kChunkHeight)
    return reject(client, "door: outside world height");
  BlockType hit = world_.getBlockAt(glm::vec3(cmd.pos) + 0.5f);
  if (!isDoor(hit)) return reject(client, "door: that isn't a door");
  if (!inReach(*ps, cmd.pos)) return reject(client, "door: out of reach");

  const glm::ivec3 bottom = cmd.pos - glm::ivec3(0, doorTop(hit) ? 1 : 0, 0);
  const BlockType swung = doorBlock(false, doorFacing(hit), !doorOpen(hit));
  // Shutting it on someone would trap them in a solid cell.
  if (!doorOpen(swung) &&
      (overlapsAnyPlayer(bottom) || overlapsAnyPlayer(bottom + glm::ivec3(0, 1, 0))))
    return reject(client, "door: someone's in the way");
  setDoor(bottom, swung);
}

// ─── Beds ──────────────────────────────────────────────────────────────

void Game::placeBed(Client &client, PlayerState &ps, glm::ivec3 pos) {
  if (pos.y < 1 || pos.y >= kChunkHeight) return reject(client, "place: outside world height");
  if (!inReach(ps, pos)) return reject(client, "place: out of reach");

  // The foot where you clicked, the head the way you look; both clear (not
  // even water), on something solid.
  const BedColour colour = bedItemColour(ps.inventory.slot(ps.selected_slot).id);
  const DoorFacing facing = doorFacingForYaw(ps.yaw);
  const glm::ivec3 head = pos + facingStep(facing);
  for (glm::ivec3 cell : {pos, head}) {
    int cx = static_cast<int>(std::floor(static_cast<float>(cell.x) / kChunkWidth));
    int cz = static_cast<int>(std::floor(static_cast<float>(cell.z) / kChunkDepth));
    if (!world_.getChunk(cx, cz)) return reject(client, "place: chunk not loaded");
  }
  for (glm::ivec3 cell : {pos, head}) {
    if (world_.getBlockAt(glm::vec3(cell) + 0.5f) != BlockType::AIR)
      return reject(client, "place: no room for a bed");
    if (overlapsAnyPlayer(cell)) return reject(client, "place: inside a player");
    if (overlapsBoat(cell)) return reject(client, "place: a boat is there");
    BlockType below = world_.getBlockAt(glm::vec3(cell) + glm::vec3(0.5f, -0.5f, 0.5f));
    if (!blocksMovement(below) || isDoor(below) || isBed(below))
      return reject(client, "place: a bed needs ground under it");
  }

  const BlockType foot_half = bedBlock(colour, false, facing);
  const BlockType head_half = bedBlock(colour, true, facing);
  world_.setBlockAt(pos, foot_half);
  world_.setBlockAt(head, head_half);
  broadcast(BlockChanged{pos, foot_half});
  broadcast(BlockChanged{head, head_half});
  packSoilUnder(pos);
  packSoilUnder(head);
  if (rules().consume_blocks) {
    ps.inventory.take(ps.selected_slot, 1);
    sendInventory(client, ps);
  }
}

void Game::popBedAbove(glm::ivec3 below) {
  const glm::ivec3 cell = below + glm::ivec3(0, 1, 0);
  BlockType b = world_.getBlockAt(glm::vec3(cell) + 0.5f);
  if (!isBed(b)) return;
  for (glm::ivec3 half : {cell, bedOther(cell, b)}) {
    if (!isBed(world_.getBlockAt(glm::vec3(half) + 0.5f))) continue;
    world_.setBlockAt(half, BlockType::AIR);
    broadcast(BlockChanged{half, BlockType::AIR});
  }
  if (rules().block_drops)
    dropItem({bedItem(bedColour(b)), 1}, glm::vec3(cell) + glm::vec3(0.5f, 0.3f, 0.5f),
             {0.0f, 3.0f, 0.0f}, kBreakDropDelayMs);
}

namespace {

// The camera yaw (degrees) that looks along a facing.
float yawOf(DoorFacing facing) {
  switch (facing) {
    case DoorFacing::North: return -90.0f;
    case DoorFacing::East:  return 0.0f;
    case DoorFacing::South: return 90.0f;
    case DoorFacing::West:
    default:                return 180.0f;
  }
}

} // namespace

bool Game::bedStillAt(glm::ivec3 foot) const {
  // A bed only ever comes from an edit, so an unloaded chunk's edits know.
  int cx = static_cast<int>(std::floor(static_cast<float>(foot.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(foot.z) / kChunkDepth));
  BlockType b = world_.getChunk(cx, cz) ? world_.getBlockAt(glm::vec3(foot) + 0.5f)
                                        : world_.editAt(foot).value_or(BlockType::AIR);
  return isBed(b) && !bedHead(b);
}

glm::vec3 Game::besideBed(glm::ivec3 foot) const {
  BlockType b = world_.getBlockAt(glm::vec3(foot) + 0.5f);
  if (isBed(b)) {
    const glm::ivec3 head = bedOther(foot, b);
    const glm::ivec3 along = head - foot;
    const glm::ivec3 across(along.z, 0, -along.x);
    // Beside the foot, then beside the head, then off either end.
    const glm::ivec3 spots[] = {foot + across, foot - across, head + across,
                                head - across, foot - along, head + along};
    for (glm::ivec3 c : spots) {
      auto at = [&](int dy) { return world_.getBlockAt(glm::vec3(c) + glm::vec3(0.5f, dy + 0.5f, 0.5f)); };
      if (!blocksMovement(at(0)) && !blocksMovement(at(1)) && !isLiquid(at(0)) &&
          blocksMovement(at(-1)) && !isBed(at(-1)))
        return glm::vec3(c) + glm::vec3(0.5f, 0.0f, 0.5f);
    }
  }
  return glm::vec3(foot) + glm::vec3(0.5f, kBedHeight, 0.5f);
}

Game::Client *Game::clientOf(PlayerId id) {
  for (Client &c : clients)
    if (c.player == id) return &c;
  return nullptr;
}

void Game::apply(Client &client, const UseBed &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead() || ps->sleeping_in) return;
  if (ps->riding) return reject(client, "bed: in a boat");
  if (cmd.pos.y < 0 || cmd.pos.y >= kChunkHeight) return reject(client, "bed: outside world height");
  BlockType hit = world_.getBlockAt(glm::vec3(cmd.pos) + 0.5f);
  if (!isBed(hit)) return reject(client, "bed: that isn't a bed");
  if (!inReach(*ps, cmd.pos)) return reject(client, "bed: out of reach");

  // It's their respawn point now, whether or not they can sleep.
  const glm::ivec3 foot = bedFoot(cmd.pos, hit);
  const bool new_spawn = ps->bed != foot;
  ps->bed = foot;
  auto refuse = [&](std::string why) {
    client.link->deliver(Notice{new_spawn ? "Respawn point set. " + why : why});
  };

  if (!isNight(clock_.fractionOfDay())) return refuse("You can only sleep at night");
  const glm::vec3 middle =
      glm::vec3(foot) + glm::vec3(0.5f, kBedHeight, 0.5f) + glm::vec3(facingStep(bedFacing(hit))) * 0.5f;
  for (const auto &[eid, e] : entities_.all())
    if (isEnemy(e.kind) && glm::distance(e.body.position, middle) <= kSleepEnemyRange)
      return refuse("You can't sleep, monsters are nearby!");
  for (const auto &[pid, other] : players)
    if (other.sleeping_in == foot) return refuse("This bed is taken");

  // Down they go: on the bed, head on the pillow. The client jumps there,
  // and moves sent before it are stale.
  if (new_spawn) client.link->deliver(Notice{"Respawn point set"});
  ps->sleeping_in = foot;
  ps->sleep_ms = 0;
  ps->sleep_health = ps->vitals.health();
  ps->breaking.reset();
  ps->position = middle;
  ps->yaw = yawOf(bedFacing(hit));
  ps->crouch = false;
  ps->fall.reset();
  ++ps->respawn;
  client.link->deliver(Respawned{ps->position, ps->respawn});
  int living = 0, sleepers = 0;
  for (const auto &[pid, other] : players) {
    if (other.vitals.dead()) continue;
    ++living;
    sleepers += other.sleeping_in.has_value();
  }
  postChat(0, "", ps->name + " is sleeping (" + std::to_string(sleepers) + " of " +
                      std::to_string(living) + ")");
  flushSleep();
}

void Game::apply(Client &client, const LeaveBed &) {
  PlayerState *ps = playerFor(client);
  if (ps && ps->sleeping_in) wake(client, *ps);
}

void Game::wake(Client &client, PlayerState &ps) {
  const glm::ivec3 foot = *ps.sleeping_in;
  ps.sleeping_in.reset();
  ps.sleep_ms = 0;
  ps.position = besideBed(foot);
  ps.fall.reset();
  ++ps.respawn;
  client.link->deliver(Respawned{ps.position, ps.respawn});
  flushSleep();
}

void Game::tickSleep() {
  const bool night = isNight(clock_.fractionOfDay());
  int living = 0, rested = 0;
  for (auto &[pid, ps] : players) {
    if (ps.vitals.dead()) {
      ps.sleeping_in.reset(); // died in bed: nothing to wake from
      continue;
    }
    ++living;
    if (!ps.sleeping_in) continue;
    // The bed went, they got hurt, or the night's over on its own.
    if (!bedStillAt(*ps.sleeping_in) || ps.vitals.health() < ps.sleep_health || !night) {
      if (Client *c = clientOf(pid)) wake(*c, ps);
      continue;
    }
    ps.sleep_health = ps.vitals.health();
    ps.sleep_ms += kTickMs;
    if (ps.sleep_ms >= kSleepMs) ++rested;
  }
  if (living == 0 || rested < living) return;

  // Everyone's asleep: morning, and up they get.
  clock_.time_of_day = kWakeTime;
  for (auto &[pid, ps] : players)
    if (ps.sleeping_in)
      if (Client *c = clientOf(pid)) wake(*c, ps);
  for (Client &c : clients)
    if (c.player && c.role == Role::Guest)
      c.link->deliver(TimeSync{clock_.time_of_day, clock_.scale});
}

void Game::flushSleep() {
  uint8_t living = 0, sleepers = 0;
  for (const auto &[pid, ps] : players) {
    if (ps.vitals.dead()) continue;
    ++living;
    sleepers += ps.sleeping_in.has_value();
  }
  for (Client &client : clients) {
    PlayerState *ps = playerFor(client);
    if (!ps) continue;
    SleepState now{ps->sleeping_in.has_value(), ps->sleeping_in.value_or(glm::ivec3(0)),
                   sleepers, living};
    if (ps->sleep_sent == now) continue;
    // Nobody in bed, and never was: nothing worth sending.
    if (!ps->sleep_sent && sleepers == 0) continue;
    ps->sleep_sent = now;
    client.link->deliver(now);
  }
}

// ─── Map ───────────────────────────────────────────────────────────────

namespace {
// How close (in blocks) a player has to stand to pick a pin back up.
constexpr int kPinReach = 3;
} // namespace

void Game::apply(Client &client, const TogglePin &) {
  PlayerState *ps = playerFor(client);
  if (!ps) return;
  if (ps->vitals.dead()) return reject(client, "pin: dead");
  const glm::ivec3 here(glm::floor(ps->position));
  auto &pins = ps->pins;
  auto near = std::find_if(pins.begin(), pins.end(), [&](const Pin &p) {
    const glm::ivec3 d = p.pos - here;
    return d.x * d.x + d.y * d.y + d.z * d.z <= kPinReach * kPinReach;
  });
  if (near != pins.end()) {
    pins.erase(near);
    return client.link->deliver(Notice{"Pin picked up"});
  }
  if (pins.size() >= kMaxPins)
    return client.link->deliver(Notice{"All your pins are out. Pick one up first"});
  // The first colour not in use, so a pin keeps its colour while others come and go.
  uint8_t colour = 0;
  while (std::any_of(pins.begin(), pins.end(), [&](const Pin &p) { return p.colour == colour; }))
    ++colour;
  pins.push_back({here, colour});
  client.link->deliver(Notice{"Pin dropped"});
}

glm::ivec3 Game::home(const PlayerState &ps) {
  if (ps.bed && bedStillAt(*ps.bed)) return *ps.bed;
  return glm::ivec3(glm::floor(respawnPoint()));
}

void Game::flushWaypoints() {
  for (Client &client : clients) {
    PlayerState *ps = playerFor(client);
    if (!ps) continue;
    Waypoints now{home(*ps), ps->pins};
    if (ps->waypoints_sent == now) continue;
    ps->waypoints_sent = now;
    client.link->deliver(now);
  }
}

// ─── Farming ───────────────────────────────────────────────────────────

namespace {
constexpr glm::ivec3 kUp{0, 1, 0};

bool chunkLoaded(const World &world, glm::ivec3 pos) {
  int cx = static_cast<int>(std::floor(static_cast<float>(pos.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(pos.z) / kChunkDepth));
  return world.getChunk(cx, cz) != nullptr;
}
} // namespace

void Game::apply(Client &client, const UseItemOn &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  if (ps->sleeping_in) return reject(client, "use: in bed");
  if (ps->riding) return reject(client, "use: in a boat");
  if (cmd.pos.y < 0 || cmd.pos.y + 1 >= kChunkHeight)
    return reject(client, "use: outside world height");
  if (!inReach(*ps, cmd.pos)) return reject(client, "use: out of reach");
  if (!chunkLoaded(world_, cmd.pos)) return reject(client, "use: chunk not loaded");

  const ItemId held = ps->inventory.slot(ps->selected_slot).id;
  if (held == ItemId::BOAT) return placeBoat(client, *ps, cmd.pos);
  if (toolOf(held).kind == ToolKind::Hoe) return till(client, *ps, cmd.pos);
  if (cropForSeed(held)) return plant(client, *ps, cmd.pos);
  if (held == ItemId::WATERING_CAN) return water(client, *ps, cmd.pos);
  reject(client, "use: nothing to use");
}

void Game::till(Client &client, PlayerState &ps, glm::ivec3 pos) {
  auto at = [&](glm::ivec3 p) { return world_.getBlockAt(glm::vec3(p) + 0.5f); };
  const BlockType target = at(pos);
  if (!tillable(target)) return reject(client, "till: only grass and dirt");
  if (at(pos + kUp) != BlockType::AIR) return reject(client, "till: something's on it");

  const Soil soil{0, nearWater(pos, at)};
  const BlockType tilled = soil.by_water ? BlockType::FARMLAND_WET : BlockType::FARMLAND_DRY;
  world_.setBlockAt(pos, tilled);
  broadcast(BlockChanged{pos, tilled});
  soil_[pos] = soil;
  // Grass sometimes turns up a seed, which pops out of the soil.
  if (rules().block_drops)
    if (auto seed = seedFromTilling(target, rng_))
      dropItem({*seed, 1}, glm::vec3(pos) + glm::vec3(0.5f, 1.1f, 0.5f), {0.0f, 3.0f, 0.0f},
               kBreakDropDelayMs);
  if (rules().timed_mining && wearHeldTool(client, ps)) sendInventory(client, ps);
}

void Game::plant(Client &client, PlayerState &ps, glm::ivec3 pos) {
  auto at = [&](glm::ivec3 p) { return world_.getBlockAt(glm::vec3(p) + 0.5f); };
  if (!isFarmland(at(pos))) return reject(client, "plant: seeds go on farmland");
  const glm::ivec3 cell = pos + kUp;
  if (at(cell) != BlockType::AIR) return reject(client, "plant: something's there");

  const BlockType sprout = cropBlock(*cropForSeed(ps.inventory.slot(ps.selected_slot).id), 0);
  world_.setBlockAt(cell, sprout);
  broadcast(BlockChanged{cell, sprout});
  crops_[cell] = {stageMs(rng_)};
  if (rules().consume_blocks) {
    ps.inventory.take(ps.selected_slot, 1);
    sendInventory(client, ps);
  }
}

void Game::water(Client &client, PlayerState &ps, glm::ivec3 pos) {
  auto at = [&](glm::ivec3 p) { return world_.getBlockAt(glm::vec3(p) + 0.5f); };
  ItemStack can = ps.inventory.slot(ps.selected_slot);
  const BlockType target = at(pos);
  // Dipped in water, it fills up.
  if (target == BlockType::WATER) {
    if (can.wear == 0) return;
    can.wear = 0;
    ps.inventory.set(ps.selected_slot, can);
    return sendInventory(client, ps);
  }
  // Poured on a crop, it waters the soil the crop is in.
  const glm::ivec3 centre = isCrop(target) ? pos - kUp : pos;
  if (!isFarmland(at(centre))) return reject(client, "water: only on farmland");
  if (rules().timed_mining && can.wear >= kCanSplashes)
    return reject(client, "water: the can is empty");

  for (int dz = -kSplashRadius; dz <= kSplashRadius; ++dz)
    for (int dx = -kSplashRadius; dx <= kSplashRadius; ++dx) {
      const glm::ivec3 p = centre + glm::ivec3(dx, 0, dz);
      const BlockType b = at(p);
      if (!isFarmland(b)) continue;
      soil_[p].wet_ms = kWetMs;
      if (b == BlockType::FARMLAND_DRY) setFarmBlock(p, BlockType::FARMLAND_WET);
    }
  // Survival: each splash uses some of the water; it empties, but never breaks.
  if (rules().timed_mining) {
    ++can.wear;
    ps.inventory.set(ps.selected_slot, can);
    sendInventory(client, ps);
  }
}

void Game::growFarms(int ms) {
  auto at = [&](glm::ivec3 p) { return world_.getBlockAt(glm::vec3(p) + 0.5f); };
  // Soil first: what dries now stops what grows on it now.
  for (auto it = soil_.begin(); it != soil_.end();) {
    const glm::ivec3 pos = it->first;
    Soil &soil = it->second;
    const std::optional<BlockType> b = farmBlockAt(pos);
    if (!b || !isFarmland(*b)) {
      it = soil_.erase(it);
      continue;
    }
    // Water can only be looked for where the world is loaded; elsewhere the
    // soil keeps what it last knew.
    if (chunkLoaded(world_, pos)) soil.by_water = nearWater(pos, at);
    soil.wet_ms = std::max(0, soil.wet_ms - ms);
    const BlockType want =
        soil.by_water || soil.wet_ms > 0 ? BlockType::FARMLAND_WET : BlockType::FARMLAND_DRY;
    if (*b != want) setFarmBlock(pos, want);
    ++it;
  }
  for (auto it = crops_.begin(); it != crops_.end();) {
    const glm::ivec3 pos = it->first;
    const std::optional<BlockType> b = farmBlockAt(pos);
    if (!b || !isCrop(*b) || cropRipe(*b)) {
      it = crops_.erase(it);
      continue;
    }
    // On dry soil it waits.
    if (farmBlockAt(pos - kUp) != BlockType::FARMLAND_WET || (it->second.left_ms -= ms) > 0) {
      ++it;
      continue;
    }
    const BlockType next = cropBlock(cropKind(*b), cropStage(*b) + 1);
    setFarmBlock(pos, next);
    if (cropRipe(next)) {
      it = crops_.erase(it);
      continue;
    }
    it->second.left_ms += stageMs(rng_);
    ++it;
  }
}

std::optional<int> Game::cropTimeLeft(glm::ivec3 pos) const {
  auto it = crops_.find(pos);
  if (it == crops_.end()) return std::nullopt;
  return it->second.left_ms;
}

std::optional<BlockType> Game::farmBlockAt(glm::ivec3 pos) const {
  if (chunkLoaded(world_, pos)) return world_.getBlockAt(glm::vec3(pos) + 0.5f);
  return world_.editAt(pos);
}

void Game::setFarmBlock(glm::ivec3 pos, BlockType type) {
  world_.applyEdit(pos, type); // kept for when the chunk loads, if it isn't
  broadcast(BlockChanged{pos, type});
}

void Game::popCropAbove(glm::ivec3 below) {
  const glm::ivec3 cell = below + kUp;
  const BlockType b = world_.getBlockAt(glm::vec3(cell) + 0.5f);
  if (!isCrop(b)) return;
  world_.setBlockAt(cell, BlockType::AIR);
  broadcast(BlockChanged{cell, BlockType::AIR});
  crops_.erase(cell);
  if (rules().block_drops)
    dropItem({seedFor(cropKind(b)), 1}, glm::vec3(cell) + glm::vec3(0.5f, 0.3f, 0.5f),
             {0.0f, 3.0f, 0.0f}, kBreakDropDelayMs);
}

void Game::popUnsupported(glm::ivec3 gone) {
  const BlockType ground = world_.getBlockAt(glm::vec3(gone) + 0.5f);
  const glm::ivec3 around[] = {{0, 1, 0}, {0, 0, -1}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}};
  for (glm::ivec3 step : around) {
    const glm::ivec3 cell = gone + step;
    if (cell.y < 0 || cell.y >= kChunkHeight) continue;
    const BlockType b = world_.getBlockAt(glm::vec3(cell) + 0.5f);
    // Only what `gone` held up: the thing above, or a torch on its side.
    if (!needsSupport(b) || cell + supportOf(b) != gone) continue;
    if (growsOn(b, ground, isSolidGround(ground))) continue;
    world_.setBlockAt(cell, BlockType::AIR);
    broadcast(BlockChanged{cell, BlockType::AIR});
    crops_.erase(cell); // a picked bush's regrowing
    if (rules().block_drops)
      if (std::optional<ItemId> drop = dropOf(b))
        dropItem({*drop, 1}, glm::vec3(cell) + glm::vec3(0.5f, 0.3f, 0.5f), {0.0f, 3.0f, 0.0f},
                 kBreakDropDelayMs);
    popUnsupported(cell); // a cactus stack goes all the way up
  }
}

std::optional<ItemId> Game::dropOf(BlockType b) {
  constexpr int kGrassSeedOdds = 8; // one in this many
  if (b == BlockType::TALL_GRASS)
    return std::uniform_int_distribution<int>(1, kGrassSeedOdds)(rng_) == 1
               ? std::optional(ItemId::WHEAT_SEEDS)
               : std::nullopt;
  return dropFor(b);
}

void Game::packSoilUnder(glm::ivec3 placed) {
  const glm::ivec3 below = placed - kUp;
  if (!isFarmland(world_.getBlockAt(glm::vec3(below) + 0.5f))) return;
  if (!blocksMovement(world_.getBlockAt(glm::vec3(placed) + 0.5f))) return;
  world_.setBlockAt(below, BlockType::DIRT);
  broadcast(BlockChanged{below, BlockType::DIRT});
  soil_.erase(below);
}

void Game::giveOrDrop(PlayerState &ps, ItemStack stack, glm::ivec3 pos) {
  ItemStack left = ps.inventory.add(stack);
  // No room: it pops out of the block instead.
  if (left.empty()) return;
  std::uniform_real_distribution<float> side(-1.0f, 1.0f);
  dropItem(left, glm::vec3(pos) + glm::vec3(0.5f, 0.3f, 0.5f), {side(rng_), 4.0f, side(rng_)},
           kBreakDropDelayMs);
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

bool isContainerSlot(int slot) {
  return slot >= kContainerSlot0 && slot < kContainerSlot0 + kChestSlots;
}
bool isFurnaceSlot(int slot) { return slot >= kFurnaceInput && slot <= kFurnaceOutput; }

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

  // To or from the open furnace or chest. A chest's slots take anything.
  Furnace *f = openFurnace(*ps);
  Chest *chest = f ? nullptr : openChest(*ps);
  if (!f && !chest) return reject(client, "move: nothing open");
  auto slotOf = [&](int s) -> ItemStack * {
    if (f) return furnaceSlot(*f, s);
    return &(*chest)[s - kContainerSlot0];
  };
  auto takes = [&](int s, ItemId id) { return !f || accepts(s, id); };
  auto valid = [&](int s) {
    return Inventory::validSlot(s) || (f ? isFurnaceSlot(s) : isContainerSlot(s));
  };
  if (!valid(cmd.from) || !valid(cmd.to) || cmd.from == cmd.to)
    return reject(client, "move: not allowed");
  ItemStack src = isContainerSlot(cmd.from) ? *slotOf(cmd.from) : ps->inventory.slot(cmd.from);
  ItemStack dst = isContainerSlot(cmd.to) ? *slotOf(cmd.to) : ps->inventory.slot(cmd.to);
  bool swapping = !dst.empty() && dst.id != src.id;
  if (src.empty() || !takes(cmd.to, src.id) || (swapping && !takes(cmd.from, dst.id)) ||
      !moveStack(src, dst, cmd.count))
    return reject(client, "move: not allowed");

  auto store = [&](int slot, const ItemStack &s) {
    if (isContainerSlot(slot)) *slotOf(slot) = s;
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
  if (b == BlockType::TREASURE_CHEST) {
    if (!unlockTreasure(client, *ps, cmd.pos))
      return reject(client, "open: locked (needs a Treasure Key)");
  } else if (b == BlockType::CHEST) {
    chests_.try_emplace(cmd.pos); // a chest block with no record yet starts empty
  } else if (b == BlockType::FURNACE || b == BlockType::FURNACE_LIT) {
    furnaces_.try_emplace(cmd.pos); // a furnace block with no record yet starts empty
  } else {
    return reject(client, "open: that isn't a furnace or a chest");
  }
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

Chest *Game::openChest(const PlayerState &ps) {
  if (!ps.open_container || !inReach(ps, *ps.open_container)) return nullptr;
  return chestAt(*ps.open_container);
}

void Game::sendContainer(Client &client, glm::ivec3 pos) {
  ContainerView view;
  view.pos = pos;
  if (const Chest *c = chestAt(pos)) {
    view.kind = ContainerKind::Chest;
    view.slots.assign(c->begin(), c->end());
  } else if (const Furnace *f = furnaceAt(pos)) {
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
  if (ps->riding) return reject(client, "attack: in a boat");
  Entity *e = entities_.find(cmd.id);
  if (!e) return reject(client, "attack: no such entity");
  if (e->kind == EntityKind::Boat) return pickUpBoat(client, *ps, cmd.id);
  const EntityTypeDef &def = entityType(e->kind);
  if (def.max_health <= 0) return reject(client, "attack: can't hit that");
  if (ps->attack_cooldown_ms > 0) return reject(client, "attack: too soon");

  if (!inReachOf(*ps, *e)) return reject(client, "attack: out of reach");

  ps->attack_cooldown_ms = kAttackCooldownMs;
  const ItemId held = ps->inventory.slot(ps->selected_slot).id;
  e->health -= attackDamage(held);
  e->hurt_ms = kHurtFlashMs;
  if (held == ItemId::RUBY_SWORD) e->burning_ms = kBurnMs; // and it catches fire
  if (rules().timed_mining && wearHeldTool(client, *ps)) sendInventory(client, *ps);

  // Knock it away from the attacker (straight back if they overlap).
  glm::vec2 away(e->body.position.x - ps->position.x, e->body.position.z - ps->position.z);
  if (glm::length(away) < 1e-3f) {
    float yaw = glm::radians(ps->yaw);
    away = {std::cos(yaw), std::sin(yaw)};
  }
  away = glm::normalize(away) * kKnockback;
  e->body.velocity = {away.x, kKnockUp, away.y};
  e->body.on_ground = false;

  if (e->health <= 0) killEntity(cmd.id);
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
  if (held.empty() ||
      std::find(animal.eats.begin(), animal.eats.end(), held.id) == animal.eats.end())
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
  if (ps->vitals.health() >= Vitals::kMax && ps->vitals.fedMs() >= Vitals::kMaxFedMs)
    return reject(client, "eat: not hungry");

  int heal = itemDef(held.id).food_heal;
  ps->inventory.take(ps->selected_slot, 1);
  sendInventory(client, *ps);
  ps->vitals.heal(heal);
  ps->vitals.feed(heal);
}

// ─── Fishing ───────────────────────────────────────────────────────────

namespace {

bool inWaterAt(const World &world, glm::vec3 p) { return isLiquid(world.getBlockAt(p)); }

} // namespace

void Game::apply(Client &client, const CastLine &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  if (ps->inventory.slot(ps->selected_slot).id != ItemId::FISHING_ROD)
    return reject(client, "cast: no fishing rod held");
  glm::vec3 dir = cmd.dir;
  if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(dir.z) ||
      glm::length(dir) < 0.1f)
    return reject(client, "cast: bad direction");
  dir = glm::normalize(dir);

  endLine(*ps); // one line each: a new cast replaces the old
  auto id = entities_.spawn(EntityKind::Bobber, ps->eyePosition() + dir * 0.6f);
  if (!id) return reject(client, "cast: entity cap reached");
  Entity &bobber = *entities_.find(*id);
  bobber.owner = ps->id;
  bobber.body.swims = true; // no sinking: it floats
  bobber.body.glides = true;
  bobber.body.velocity = dir * kCastSpeed + glm::vec3(0.0f, kCastLift, 0.0f);
  ps->fishing = PlayerState::Fishing{*id};
}

void Game::apply(Client &client, const ReelIn &) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead()) return;
  if (!ps->fishing) return reject(client, "reel: no line out");
  const Entity *bobber = entities_.find(ps->fishing->bobber);
  bool bite = bobber && ps->fishing->window_end_ms != 0;
  glm::vec3 at = bobber ? bobber->body.position : glm::vec3(0.0f);
  endLine(*ps);
  if (!bite) return; // too early, or on land: nothing on the hook

  glm::ivec3 cell(std::floor(at.x), std::floor(at.y + 0.1f), std::floor(at.z));
  ItemStack got = rollCatch(world_, cell, rng_);
  ItemStack left = ps->inventory.add(got);
  if (!left.empty())
    dropItem(left, ps->position + glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f),
             kBreakDropDelayMs);
  client.link->deliver(Caught{got.id});
  if (rules().timed_mining) spendUse(client, *ps);
  sendInventory(client, *ps);
}

void Game::endLine(PlayerState &ps) {
  if (!ps.fishing) return;
  entities_.remove(ps.fishing->bobber);
  ps.fishing.reset();
}

void Game::tickFishing() {
  const uint64_t now = nowMs();
  for (auto &[pid, ps] : players) {
    if (!ps.fishing) continue;
    PlayerState::Fishing &line = *ps.fishing;
    Entity *bobber = entities_.find(line.bobber);
    // The line breaks: switching away from the rod, walking off, dying.
    if (!bobber || ps.vitals.dead() ||
        ps.inventory.slot(ps.selected_slot).id != ItemId::FISHING_ROD ||
        glm::distance(ps.position, bobber->body.position) > kLineBreakDistance) {
      endLine(ps);
      continue;
    }

    PhysicsBody &body = bobber->body;
    if (!inWaterAt(world_, body.position + glm::vec3(0.0f, 0.1f, 0.0f))) {
      // On land or still flying: nothing will bite.
      line.bite_at_ms = line.window_end_ms = 0;
      bobber->biting = false;
      continue;
    }
    // Float at the top of the water, a little sunk.
    glm::vec3 top = body.position + glm::vec3(0.0f, 0.1f, 0.0f);
    while (inWaterAt(world_, top + glm::vec3(0.0f, 1.0f, 0.0f))) top.y += 1.0f;
    body.position.y = std::floor(top.y) + 1.0f - 0.12f;
    body.velocity = glm::vec3(0.0f);

    auto rollWait = [&] {
      bool near = false;
      for (const auto &[id, e] : entities_.all())
        near |= isFish(e.kind) && glm::distance(e.body.position, body.position) <= kFishNearBobber;
      line.bite_at_ms = now + biteWaitMs(rng_, near);
    };
    if (line.bite_at_ms == 0) rollWait();
    if (line.window_end_ms == 0 && now >= line.bite_at_ms) {
      line.window_end_ms = now + kBiteWindowMs;
    } else if (line.window_end_ms != 0 && now > line.window_end_ms + kBreakSlackMs) {
      line.window_end_ms = 0; // missed it: the fish swims off
      rollWait();
    }
    bobber->biting = line.window_end_ms != 0;
  }
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
      ps->hurt_by = "was beaten by a " + std::string(entityType(e.kind).name);
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

// ─── Boats ─────────────────────────────────────────────────────────────

namespace {

// A boat's box at `pos`.
std::pair<glm::vec3, glm::vec3> boatBox(glm::vec3 pos) {
  const EntityTypeDef &def = entityType(EntityKind::Boat);
  const glm::vec3 half(def.half_width, 0.0f, def.half_width);
  return {pos - half, pos + half + glm::vec3(0.0f, def.height, 0.0f)};
}

bool boxesOverlap(std::pair<glm::vec3, glm::vec3> a, std::pair<glm::vec3, glm::vec3> b) {
  return a.first.x < b.second.x && a.second.x > b.first.x && a.first.y < b.second.y &&
         a.second.y > b.first.y && a.first.z < b.second.z && a.second.z > b.first.z;
}

} // namespace

void Game::placeBoat(Client &client, PlayerState &ps, glm::ivec3 pos) {
  auto at = [&](glm::ivec3 p) { return world_.getBlockAt(glm::vec3(p) + 0.5f); };
  if (!isLiquid(at(pos))) return reject(client, "boat: boats go on water");
  // Aimed at the water from below its top: it goes on the top.
  glm::ivec3 top = pos;
  while (top.y + 1 < kChunkHeight && isLiquid(at(top + glm::ivec3(0, 1, 0)))) ++top.y;
  const glm::vec3 spot{top.x + 0.5f, floatHeight(top.y + 1.0f), top.z + 0.5f};

  const EntityTypeDef &def = entityType(EntityKind::Boat);
  PhysicsBody body;
  body.half_width = def.half_width;
  body.height = def.height;
  if (body.collidesAt(&world_, spot)) return reject(client, "boat: no room");
  const auto box = boatBox(spot);
  for (const auto &[id, e] : entities_.all())
    if (e.kind == EntityKind::Boat && boxesOverlap(box, boatBox(e.body.position)))
      return reject(client, "boat: there's a boat there");
  for (const auto &[pid, other] : players) {
    const glm::vec3 half(kPlayerHalfWidth, 0.0f, kPlayerHalfWidth);
    const auto player = std::pair(other.position - half,
                                  other.position + half + glm::vec3(0.0f, other.height(), 0.0f));
    if (boxesOverlap(box, player)) return reject(client, "boat: a player is there");
  }

  auto id = entities_.spawn(EntityKind::Boat, spot);
  if (!id) return reject(client, "boat: entity cap reached");
  entities_.find(*id)->yaw = std::fmod(boatYawFor(ps.yaw) + 360.0f, 360.0f);
  if (rules().consume_blocks) {
    ps.inventory.take(ps.selected_slot, 1);
    sendInventory(client, ps);
  }
}

void Game::pickUpBoat(Client &client, PlayerState &ps, EntityId id) {
  const Entity *boat = entities_.find(id);
  if (!inReachOf(ps, *boat)) return reject(client, "boat: out of reach");
  for (int seat = 0; seat < kBoatSeats; ++seat)
    if (riderIn(id, seat)) return reject(client, "boat: someone's in it");
  const glm::ivec3 cell = glm::floor(boat->body.position + glm::vec3(0.0f, 0.5f, 0.0f));
  entities_.remove(id);
  saved_seats.erase(id);
  giveOrDrop(ps, {ItemId::BOAT, 1}, cell);
  sendInventory(client, ps);
}

PlayerState *Game::riderIn(EntityId boat, int seat) {
  for (auto &[pid, ps] : players)
    if (ps.riding && ps.riding->boat == boat && ps.riding->seat == seat) return &ps;
  return nullptr;
}

bool Game::overlapsBoat(const glm::ivec3 &block) const {
  const auto cube = std::pair(glm::vec3(block), glm::vec3(block) + 1.0f);
  for (const auto &[id, e] : entities_.all())
    if (e.kind == EntityKind::Boat && boxesOverlap(cube, boatBox(e.body.position))) return true;
  return false;
}

void Game::seatRiders(const Entity &boat) {
  for (auto &[pid, ps] : players)
    if (ps.riding && ps.riding->boat == boat.id)
      ps.position = seatPosition(boat.body.position, boat.yaw, ps.riding->seat);
}

void Game::apply(Client &client, const BoardBoat &cmd) {
  PlayerState *ps = playerFor(client);
  if (!ps || ps->vitals.dead() || ps->sleeping_in) return;
  if (ps->riding) return reject(client, "board: already in a boat");
  const Entity *boat = entities_.find(cmd.id);
  if (!boat || boat->kind != EntityKind::Boat) return reject(client, "board: that isn't a boat");
  if (!inReachOf(*ps, *boat)) return reject(client, "board: out of reach");
  // The first one in drives; the driver's seat is free again once they leave.
  int seat = 0;
  while (seat < kBoatSeats && riderIn(cmd.id, seat)) ++seat;
  if (seat == kBoatSeats) return reject(client, "board: the boat is full");

  ps->riding = PlayerState::Riding{cmd.id, seat};
  ps->breaking.reset();
  ps->crouch = false;
  ps->fall.reset();
  ps->position = seatPosition(boat->body.position, boat->yaw, seat);
  // Moves sent before they sat down are stale.
  ++ps->respawn;
  client.link->deliver(Riding{cmd.id, static_cast<uint8_t>(seat), boat->body.position, boat->yaw});
  client.link->deliver(Respawned{ps->position, ps->respawn});
}

void Game::apply(Client &client, const LeaveBoat &) {
  PlayerState *ps = playerFor(client);
  if (ps && ps->riding) unseat(client, *ps);
}

void Game::unseat(Client &client, PlayerState &ps) {
  const Entity *boat = entities_.find(ps.riding->boat);
  ps.riding.reset();
  ps.position = boat ? besideBoat(boat->body.position, boat->yaw) : ps.position;
  ps.fall.reset();
  ++ps.respawn;
  client.link->deliver(Riding{});
  client.link->deliver(Respawned{ps.position, ps.respawn});
}

glm::vec3 Game::besideBoat(glm::vec3 boat, float yaw) const {
  auto blocks = [&](glm::ivec3 c) { return blocksMovement(world_.getBlockAt(glm::vec3(c) + 0.5f)); };
  auto liquid = [&](glm::ivec3 c) { return isLiquid(world_.getBlockAt(glm::vec3(c) + 0.5f)); };
  const glm::ivec3 up(0, 1, 0);
  const glm::vec3 ahead = boatForward(yaw), side(-ahead.z, 0.0f, ahead.x);
  const glm::vec3 around[] = {side * 1.2f,           -side * 1.2f,
                              ahead * 1.5f,          -ahead * 1.5f,
                              (side + ahead) * 1.1f, (side - ahead) * 1.1f,
                              (ahead - side) * 1.1f, (-side - ahead) * 1.1f};
  const int hull = static_cast<int>(std::floor(boat.y + 0.05f)); // the row the hull sits in
  auto cellAt = [&](glm::vec3 offset, int y) {
    return glm::ivec3(std::floor(boat.x + offset.x), y, std::floor(boat.z + offset.z));
  };
  auto standOn = [](glm::ivec3 c, float y) { return glm::vec3(c.x + 0.5f, y, c.z + 0.5f); };

  // Dry land beside it, or a block further off (pulled up near the shore):
  // at the waterline, or a step up.
  for (float reach : {1.0f, 2.0f})
    for (int dy : {1, 2})
      for (glm::vec3 o : around) {
        glm::ivec3 c = cellAt(o * reach, hull + dy);
        if (!blocks(c) && !blocks(c + up) && !liquid(c) && blocks(c - up) && !liquid(c - up))
          return standOn(c, static_cast<float>(c.y));
      }
  // Else the water beside it, head above the surface.
  for (glm::vec3 o : around) {
    glm::ivec3 c = cellAt(o, hull);
    if (liquid(c) && !blocks(c + up) && !blocks(c + up + up))
      return standOn(c, hull + 0.4f);
  }
  // Hemmed in: straight up, to the first gap a player fits.
  for (int y = hull + 1; y + 1 < kChunkHeight; ++y) {
    glm::ivec3 c = cellAt(glm::vec3(0.0f), y);
    if (!blocks(c) && !blocks(c + up)) return standOn(c, static_cast<float>(y));
  }
  return boat + glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 Game::standingPosition(const PlayerState &ps) const {
  if (!ps.riding) return ps.position;
  const Entity *boat = entities_.find(ps.riding->boat);
  return boat ? besideBoat(boat->body.position, boat->yaw) : ps.position;
}

void Game::tickBoats() {
  for (Client &client : clients) {
    PlayerState *ps = playerFor(client);
    if (!ps || !ps->riding) continue;
    if (ps->vitals.dead()) { // they respawn on foot
      ps->riding.reset();
      client.link->deliver(Riding{});
      continue;
    }
    const Entity *boat = entities_.find(ps->riding->boat);
    if (!boat) {
      ps->riding.reset();
      client.link->deliver(Riding{});
      continue;
    }
    ps->position = seatPosition(boat->body.position, boat->yaw, ps->riding->seat);
  }
}

void Game::retakeSeat(Client &client, PlayerState &ps, bool saved_host) {
  using Rider = WorldSave::BoatRecord::Rider;
  for (auto it = saved_seats.begin(); it != saved_seats.end(); ++it)
    for (int seat = 0; seat < kBoatSeats; ++seat) {
      WorldSave::BoatRecord::Seat &s = it->second[seat];
      bool mine = (s.rider == Rider::Host && saved_host) ||
                  (s.rider == Rider::Guest && client.role == Role::Guest && s.name == ps.name);
      if (!mine) continue;
      s = {};
      const EntityId id = it->first;
      if (it->second == SavedSeats{}) saved_seats.erase(it);
      // Gone, or someone else sat down: they stay where they were saved,
      // beside it.
      const Entity *boat = entities_.find(id);
      if (!boat || riderIn(id, seat)) return;
      ps.riding = PlayerState::Riding{id, seat};
      ps.position = seatPosition(boat->body.position, boat->yaw, seat);
      ++ps.respawn;
      client.link->deliver(Riding{id, static_cast<uint8_t>(seat), boat->body.position, boat->yaw});
      client.link->deliver(Respawned{ps.position, ps.respawn});
      return;
    }
}
