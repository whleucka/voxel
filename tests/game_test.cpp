// Game is exercised through a LocalConnection exactly as Engine drives it,
// without a window: World::init() (shaders) is never called.
#include "doctest/doctest.h"
#include "block/door.hpp"
#include "item/mining.hpp"
#include "item/tools.hpp"
#include "core/settings.hpp"
#include "entity/enemies.hpp"
#include "entity/fish.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include <cmath>

namespace {

// Joining makes the Game stream chunks around the player. Radius 0 streams
// only the chunk each player stands in, so tests don't generate a full render
// distance of terrain and know exactly which chunks can appear.
struct SmallGame {
  ScopedRenderDistance rd{0};
  Game game;
  // Natural spawning would add animals mid-test; tests that want it add rules.
  SmallGame() { game.spawner() = Spawner{}; }
};

std::vector<Event> pollAll(LocalConnection &conn) {
  std::vector<Event> out;
  conn.poll(out);
  return out;
}

// Sends Join, pumps, and returns the Joined reply.
Joined join(Game &game, LocalConnection &conn) {
  game.addClient(conn);
  conn.send(Join{});
  game.pump();
  auto events = pollAll(conn);
  REQUIRE(events.size() == 4);
  REQUIRE(std::holds_alternative<Joined>(events[0]));
  REQUIRE(std::holds_alternative<InventoryContents>(events[1]));
  REQUIRE(std::holds_alternative<KnownItems>(events[2])); // the starter kit, at least
  REQUIRE(std::holds_alternative<VitalsChanged>(events[3]));
  return std::get<Joined>(events[0]);
}

} // namespace

TEST_CASE_FIXTURE(SmallGame, "advance runs whole fixed ticks and carries the remainder") {
  CHECK(game.advance(0.125f) == 2);  // 100 ms used, 25 ms carried
  CHECK(game.advance(0.025f) == 1);  // carried 25 + 25 = one more tick
  CHECK(game.advance(0.049f) == 0);
  CHECK(game.advance(0.0f) == 0);
}

TEST_CASE_FIXTURE(SmallGame, "the clock advances only on ticks") {
  float saved_scale = g_settings.time_scale;
  g_settings.time_scale = 72.0f;

  float t0 = game.clock().time_of_day;
  game.advance(0.04f); // under one tick
  CHECK(game.clock().time_of_day == t0);
  game.advance(0.02f); // crosses the tick boundary
  CHECK(game.clock().time_of_day ==
        doctest::Approx(t0 + Game::kTickSeconds * 72.0f));

  g_settings.time_scale = saved_scale;
}

TEST_CASE_FIXTURE(SmallGame, "joining a fresh world spawns on solid ground") {
  LocalConnection conn;
  Joined j = join(game, conn);

  CHECK(j.id != 0);
  REQUIRE(game.findPlayer(j.id) != nullptr);
  CHECK(game.findPlayer(j.id)->position == j.position);

  int cx = static_cast<int>(std::floor(j.position.x / kChunkWidth));
  int cz = static_cast<int>(std::floor(j.position.z / kChunkDepth));
  game.world().loadChunkSync(cx, cz);
  CHECK(game.world().isSolidBlock(static_cast<int>(std::floor(j.position.x)),
                                  static_cast<int>(j.position.y) - 1,
                                  static_cast<int>(std::floor(j.position.z))));
}

TEST_CASE_FIXTURE(SmallGame, "the first join restores the saved player") {
  WorldSave::LevelData level;
  level.time_of_day = 1000.0f;
  level.mode = GameMode::Creative; // flying is kept only where it's allowed
  WorldSave::PlayerData pd;
  pd.position = {12.5f, 90.0f, -7.5f};
  pd.yaw = 30.0f;
  pd.pitch = -10.0f;
  pd.selected_slot = 6;
  pd.fly_mode = 1;

  game.restore(level, {}, pd);
  CHECK(game.clock().time_of_day == 1000.0f);

  LocalConnection conn;
  Joined j = join(game, conn);
  CHECK(j.position == pd.position);
  CHECK(j.yaw == pd.yaw);
  CHECK(j.pitch == pd.pitch);
  CHECK(j.slot == 6);
  CHECK(j.fly);

  // A second client gets a fresh spawn, not the same saved record.
  LocalConnection conn2;
  Joined j2 = join(game, conn2);
  CHECK(j2.id != j.id);
  CHECK(j2.position != pd.position);
}

TEST_CASE_FIXTURE(SmallGame, "joining twice on one connection is ignored") {
  LocalConnection conn;
  join(game, conn);
  conn.send(Join{});
  game.pump();
  CHECK(pollAll(conn).empty());
}

TEST_CASE_FIXTURE(SmallGame, "SetTimeOfDay sets and wraps the clock") {
  LocalConnection conn;
  join(game, conn);

  conn.send(SetTimeOfDay{6 * 3600.0f});
  game.pump();
  CHECK(game.clock().time_of_day == 6 * 3600.0f);

  conn.send(SetTimeOfDay{24 * 3600.0f});
  game.pump();
  CHECK(game.clock().time_of_day == 0.0f);
}

TEST_CASE_FIXTURE(SmallGame, "MovePlayer updates the player's state") {
  LocalConnection conn;
  Joined j = join(game, conn);

  conn.send(MovePlayer{{40.5f, 70.0f, -8.5f}, 15.0f, -20.0f, true, true});
  game.pump();

  const PlayerState *ps = game.findPlayer(j.id);
  REQUIRE(ps);
  CHECK(ps->position == glm::vec3(40.5f, 70.0f, -8.5f));
  CHECK(ps->yaw == 15.0f);
  CHECK(ps->pitch == -20.0f);
  CHECK(ps->fly);
  CHECK(ps->crouch);
  CHECK(ps->height() == kPlayerCrouchHeight);
}

TEST_CASE_FIXTURE(SmallGame, "chunks stream around each player's position") {
  LocalConnection conn;
  join(game, conn);

  const glm::vec3 far{200.5f * kChunkWidth, 80.0f, 0.5f};
  conn.send(MovePlayer{far, 0.0f, 0.0f, false, false});
  game.pump();

  CHECK(waitForChunk(game.world(), 200, 0));
}

TEST_CASE_FIXTURE(SmallGame, "SelectSlot stores valid slots and rejects others") {
  LocalConnection conn;
  Joined j = join(game, conn);

  conn.send(SelectSlot{3});
  game.pump();
  CHECK(game.findPlayer(j.id)->selected_slot == 3);
  CHECK(pollAll(conn).empty());

  conn.send(SelectSlot{42});
  conn.send(SelectSlot{-1});
  game.pump();
  CHECK(game.findPlayer(j.id)->selected_slot == 3);
  auto events = pollAll(conn);
  REQUIRE(events.size() == 2);
  CHECK(std::holds_alternative<CommandRejected>(events[0]));
}

TEST_CASE_FIXTURE(SmallGame, "commands before Join are ignored") {
  LocalConnection conn;
  game.addClient(conn);
  conn.send(MovePlayer{{1.0f, 2.0f, 3.0f}, 0.0f, 0.0f, false, false});
  conn.send(SelectSlot{2});
  game.pump();
  CHECK(pollAll(conn).empty());
}

// ─── Block edits ───────────────────────────────────────────────────────

namespace {

// A joined player standing on the terrain of chunk (0,0), next to column
// (5,5). Two connections so broadcasts can be checked.
struct EditGame : SmallGame {
  LocalConnection conn, other;
  PlayerId id = 0;
  int ground = 0; // top solid block y in column (5,5)

  EditGame() {
    game.world().loadChunkSync(0, 0);
    for (int y = kChunkHeight - 1; y >= 0; --y) {
      BlockType b = game.world().getBlockAt(glm::vec3(5.5f, y + 0.5f, 5.5f));
      if (b != BlockType::AIR && b != BlockType::WATER) { ground = y; break; }
    }
    REQUIRE(ground > 0);
    id = join(game, conn).id;
    // A second player far away, only to observe broadcasts.
    game.addClient(other);
    other.send(Join{});
    game.pump();
    pollAll(other);
    pollAll(conn); // PlayerJoined about the observer
    moveTo({5.5f, ground + 1.0f, 8.5f});
  }

  void moveTo(glm::vec3 pos, bool crouch = false) {
    conn.send(MovePlayer{pos, 0.0f, 0.0f, false, crouch});
    game.pump();
  }

  BlockType at(glm::ivec3 p) const {
    return game.world().getBlockAt(glm::vec3(p) + 0.5f);
  }

  // Sends one command, pumps, and returns this client's events.
  std::vector<Event> run(Command cmd) {
    conn.send(std::move(cmd));
    game.pump();
    return pollAll(conn);
  }

  static bool rejected(const std::vector<Event> &evs) {
    return evs.size() == 1 && std::holds_alternative<CommandRejected>(evs[0]);
  }

  static int changes(const std::vector<Event> &evs) {
    int n = 0;
    for (const Event &e : evs) n += std::holds_alternative<BlockChanged>(e);
    return n;
  }

  const Inventory &inventory() const { return game.findPlayer(id)->inventory; }

  // Gives one of a block's item and selects the hotbar slot holding it.
  void hold(BlockType b) {
    run(GiveItem{itemForBlock(b), 1});
    for (int i = 0; i < kHotbarSlots; ++i)
      if (inventory().slot(i).id == itemForBlock(b)) {
        run(SelectSlot{i});
        return;
      }
    FAIL("item didn't land in the hotbar");
  }

  // Breaks a block the way a survival player does: start, keep at it for as
  // long as the block and the held tool take, then break. (Creative breaks
  // at once; the wait does no harm.)
  std::vector<Event> mine(glm::ivec3 pos) {
    run(StartBreak{pos});
    int ms = breakMs(at(pos), inventory().slot(game.findPlayer(id)->selected_slot).id);
    for (int t = 0; t < ms; t += Game::kTickMs) game.tick();
    pollAll(conn);
    pollAll(other); // snapshots and the like from the wait
    return run(BreakBlock{pos});
  }

  // Puts a tool in the hand (the selected slot), as if picked up.
  void wield(ItemId tool) {
    PlayerState &me = *game.findPlayer(id);
    me.inventory.set(me.selected_slot, {tool, 1});
  }

  // Places a block of the given type the way a player would: hold it, place.
  std::vector<Event> place(glm::ivec3 pos, BlockType b) {
    hold(b);
    return run(PlaceBlock{pos});
  }
};

} // namespace

TEST_CASE_FIXTURE(EditGame, "a valid break removes the block and broadcasts once") {
  glm::ivec3 target{5, ground, 5};
  auto evs = mine(target);

  CHECK(at(target) == BlockType::AIR);
  REQUIRE(changes(evs) == 1);
  REQUIRE(std::holds_alternative<BlockChanged>(evs[0]));
  CHECK(std::get<BlockChanged>(evs[0]).pos == target);
  CHECK(std::get<BlockChanged>(evs[0]).type == BlockType::AIR);
  CHECK(pollAll(other).size() == 1);
}

TEST_CASE_FIXTURE(EditGame, "a valid place sets the block and broadcasts once") {
  glm::ivec3 target{5, ground + 1, 5};
  auto evs = place(target, BlockType::COBBLESTONE);

  CHECK(at(target) == BlockType::COBBLESTONE);
  REQUIRE(changes(evs) == 1);
  CHECK(std::get<BlockChanged>(evs[0]).type == BlockType::COBBLESTONE);
  CHECK(pollAll(other).size() == 1);
}

TEST_CASE_FIXTURE(EditGame, "bedrock, water and air cannot be broken") {
  // Placed directly: generated bedrock isn't guaranteed at any one cell.
  glm::ivec3 bedrock{5, ground + 1, 5};
  game.world().setBlockAt(bedrock, BlockType::BEDROCK);
  CHECK(rejected(mine(bedrock)));
  CHECK(at(bedrock) == BlockType::BEDROCK);

  glm::ivec3 water{5, ground + 2, 5};
  game.world().setBlockAt(water, BlockType::WATER);
  CHECK(rejected(mine(water)));
  CHECK(at(water) == BlockType::WATER);

  CHECK(rejected(mine({5, ground + 3, 5})));
  CHECK(pollAll(other).empty());
}

TEST_CASE_FIXTURE(EditGame, "edits out of reach are rejected") {
  glm::ivec3 far{5, ground, 5};
  moveTo({5.5f, ground + 1.0f, 5.5f + 8.0f});
  CHECK(rejected(mine(far)));
  CHECK(rejected(place(far + glm::ivec3(0, 1, 0), BlockType::STONE)));
  CHECK(at(far) != BlockType::AIR);
}

TEST_CASE_FIXTURE(EditGame, "placing outside the world height is rejected") {
  moveTo({5.5f, kChunkHeight - 1.0f, 8.5f});
  CHECK(rejected(place({5, kChunkHeight, 5}, BlockType::STONE)));
  moveTo({5.5f, 0.0f, 8.5f});
  CHECK(rejected(place({5, -1, 5}, BlockType::STONE)));
}

TEST_CASE_FIXTURE(EditGame, "placing with nothing held or into an occupied cell is rejected") {
  run(SelectSlot{kHotbarSlots - 1}); // empty: the kit only fills slot 0
  CHECK(rejected(run(PlaceBlock{{5, ground + 1, 5}})));
  CHECK(at({5, ground + 1, 5}) == BlockType::AIR);
  CHECK(rejected(place({5, ground, 5}, BlockType::STONE)));
}

TEST_CASE_FIXTURE(EditGame, "placing into water replaces it") {
  glm::ivec3 water{5, ground + 1, 5};
  game.world().setBlockAt(water, BlockType::WATER);
  auto evs = place(water, BlockType::SAND);
  CHECK(at(water) == BlockType::SAND);
  CHECK(changes(evs) == 1);
}

TEST_CASE_FIXTURE(EditGame, "placing inside your own box is rejected") {
  const glm::vec3 feet{5.5f, ground + 1.0f, 5.5f};
  moveTo(feet);
  CHECK(rejected(place({5, ground + 1, 5}, BlockType::STONE)));
  CHECK(rejected(place({5, ground + 2, 5}, BlockType::STONE)));
  CHECK(changes(place({5, ground + 3, 5}, BlockType::STONE)) == 1);
}

TEST_CASE_FIXTURE(EditGame, "crouching lowers the box used for placement") {
  // Feet 0.4 into the cell: standing (1.8) reaches into ground+3,
  // crouched (1.5) stops below it.
  glm::ivec3 cell{5, ground + 3, 5};
  moveTo({5.5f, ground + 1.4f, 5.5f});
  CHECK(rejected(place(cell, BlockType::STONE)));

  moveTo({5.5f, ground + 1.4f, 5.5f}, /*crouch=*/true);
  CHECK(changes(place(cell, BlockType::STONE)) == 1);
  CHECK(at(cell) == BlockType::STONE);
}

TEST_CASE_FIXTURE(EditGame, "placing inside another player's box is rejected") {
  // Move the observer next to the target cell.
  other.send(MovePlayer{{5.5f, ground + 1.0f, 6.5f}, 0, 0, false, false});
  game.pump();
  CHECK(rejected(place({5, ground + 1, 6}, BlockType::STONE)));
}

TEST_CASE_FIXTURE(EditGame, "placing into an unloaded chunk is rejected") {
  // Both players stay in chunk (0,0), so chunk (-1,0) is never streamed.
  other.send(MovePlayer{{8.5f, ground + 1.0f, 8.5f}, 0, 0, false, false});
  moveTo({0.5f, ground + 1.0f, 5.5f});
  REQUIRE(game.world().getChunk(-1, 0) == nullptr);
  CHECK(rejected(place({-1, ground + 1, 5}, BlockType::STONE)));
}

TEST_CASE_FIXTURE(EditGame, "glowstone placed through the Game lights and unlights") {
  glm::ivec3 lamp{5, ground + 2, 5};
  REQUIRE(changes(place(lamp, BlockType::GLOWSTONE)) == 1);
  CHECK(game.world().getBlockLightAt(lamp + glm::ivec3(1, 0, 0)) > 0);

  REQUIRE(changes(mine(lamp)) == 1);
  CHECK(game.world().getBlockLightAt(lamp + glm::ivec3(1, 0, 0)) == 0);
}

// ─── Save / load ───────────────────────────────────────────────────────

TEST_CASE_FIXTURE(EditGame, "a played game round-trips through save and load") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string())); // empty dir: fresh world
  game.setMode(GameMode::Creative);            // so the saved flying is kept

  glm::ivec3 broken{5, ground, 5};
  glm::ivec3 placed{6, ground + 1, 5};
  REQUIRE(at(placed) == BlockType::AIR);
  REQUIRE(changes(mine(broken)) == 1);
  REQUIRE(changes(place(placed, BlockType::GLOWSTONE)) == 1);
  run(GiveItem{ItemId::DIAMOND, 3});
  run(MoveItems{0, 25, 0}); // glowstone into storage
  Inventory saved_inv = inventory();
  run(SelectSlot{7});
  conn.send(MovePlayer{{7.5f, ground + 1.0f, 9.5f}, 33.0f, -4.0f, true, false});
  conn.send(SetTimeOfDay{5000.0f});
  game.pump();

  // Save through the command, as F5 does.
  run(Save{});

  ScopedRenderDistance rd2{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  CHECK(loaded.clock().time_of_day == 5000.0f);
  CHECK(loaded.mode() == GameMode::Creative);

  loaded.world().loadChunkSync(0, 0);
  auto blockAt = [&](glm::ivec3 p) {
    return loaded.world().getBlockAt(glm::vec3(p) + 0.5f);
  };
  CHECK(blockAt(broken) == BlockType::AIR);
  CHECK(blockAt(placed) == BlockType::GLOWSTONE);

  LocalConnection c;
  Joined j = join(loaded, c);
  CHECK(j.position == glm::vec3(7.5f, ground + 1.0f, 9.5f));
  CHECK(j.yaw == 33.0f);
  CHECK(j.pitch == -4.0f);
  CHECK(j.slot == 7);
  CHECK(j.fly);
  CHECK(loaded.findPlayer(j.id)->inventory.slots() == saved_inv.slots());
}

// ─── Discovery (items seen) ────────────────────────────────────────────

namespace {
bool seen(const Game &game, PlayerId id, ItemId item) {
  return game.findPlayer(id)->seen.test(static_cast<size_t>(item));
}
int heldCount(const Inventory &inv, ItemId id) {
  int n = 0;
  for (const ItemStack &s : inv.slots()) n += s.id == id ? s.count : 0;
  return n;
}
} // namespace

TEST_CASE_FIXTURE(EditGame, "items count as seen once they've been in the inventory") {
  CHECK(seen(game, id, itemForBlock(BlockType::GLOWSTONE))); // the starter kit
  CHECK_FALSE(seen(game, id, ItemId::RUBY));
  run(GiveItem{ItemId::RUBY, 1});
  CHECK(seen(game, id, ItemId::RUBY));

  glm::ivec3 target{5, ground, 5};
  ItemId drop = *dropFor(at(target));
  mine(target);
  CHECK(seen(game, id, drop));

  // Still seen after it's gone.
  for (int i = 0; i < kInventorySlots; ++i)
    if (inventory().slot(i).id == ItemId::RUBY) {
      run(SelectSlot{i});
      run(DropItem{true});
    }
  REQUIRE(heldCount(inventory(), ItemId::RUBY) == 0);
  CHECK(seen(game, id, ItemId::RUBY));
}

TEST_CASE_FIXTURE(EditGame, "the host's discoveries are saved, even for items they no longer have") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string()));
  run(GiveItem{ItemId::EMERALD, 1});
  for (int i = 0; i < kHotbarSlots; ++i)
    if (inventory().slot(i).id == ItemId::EMERALD) {
      run(SelectSlot{i});
      run(DropItem{true});
    }
  run(Save{});

  ScopedRenderDistance rd2{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  LocalConnection c;
  Joined j = join(loaded, c);
  CHECK(seen(loaded, j.id, ItemId::EMERALD));
  CHECK(heldCount(loaded.findPlayer(j.id)->inventory, ItemId::EMERALD) == 0);
}

TEST_CASE_FIXTURE(EditGame, "a world from before crafting starts discoveries from the inventory") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string()));
  run(GiveItem{ItemId::DIAMOND, 2});
  run(Save{});
  std::filesystem::remove(dir.path / "discovered.dat");

  ScopedRenderDistance rd2{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  LocalConnection c;
  Joined j = join(loaded, c);
  CHECK(seen(loaded, j.id, ItemId::DIAMOND));
  CHECK_FALSE(seen(loaded, j.id, ItemId::RUBY));
}

// ─── Crafting ──────────────────────────────────────────────────────────

namespace {
constexpr uint16_t kOakPlanksRecipe = 0, kTableRecipe = 5, kStoneBricksRecipe = 6,
                   kRedRecipe = 8;

std::string reason(const std::vector<Event> &evs) {
  for (const Event &e : evs)
    if (const auto *r = std::get_if<CommandRejected>(&e)) return r->reason;
  return "";
}
} // namespace

TEST_CASE_FIXTURE(EditGame, "planks can be made anywhere, and announce themselves") {
  run(GiveItem{itemForBlock(BlockType::OAK_LOG), 2});
  auto evs = run(Craft{kOakPlanksRecipe, false});
  CHECK(heldCount(inventory(), itemForBlock(BlockType::OAK_LOG)) == 1);
  CHECK(heldCount(inventory(), itemForBlock(BlockType::OAK_PLANKS)) == 4);
  const KnownItems *known = nullptr;
  for (const Event &e : evs)
    if (const auto *k = std::get_if<KnownItems>(&e)) known = k;
  REQUIRE(known);
  CHECK(known->ids == std::vector<ItemId>{itemForBlock(BlockType::OAK_PLANKS)});
}

TEST_CASE_FIXTURE(EditGame, "\"all\" crafts as many as the ingredients allow") {
  run(GiveItem{itemForBlock(BlockType::OAK_LOG), 3});
  run(Craft{kOakPlanksRecipe, true});
  CHECK(heldCount(inventory(), itemForBlock(BlockType::OAK_LOG)) == 0);
  CHECK(heldCount(inventory(), itemForBlock(BlockType::OAK_PLANKS)) == 12);
}

TEST_CASE_FIXTURE(EditGame, "a recipe you haven't discovered is refused in survival") {
  run(GiveItem{itemForBlock(BlockType::WHITE_BLOCK), 8});
  place({5, ground + 1, 5}, BlockType::CRAFTING_TABLE);
  auto evs = run(Craft{kRedRecipe, false}); // never seen a ruby
  CHECK(reason(evs) == "craft: you haven't discovered that yet");
  CHECK(heldCount(inventory(), itemForBlock(BlockType::WHITE_BLOCK)) == 8);
}

TEST_CASE_FIXTURE(EditGame, "table recipes need a Crafting Table in reach") {
  run(GiveItem{itemForBlock(BlockType::COBBLESTONE), 4});
  CHECK(reason(run(Craft{kStoneBricksRecipe, false})) == "craft: needs a Crafting Table nearby");

  // One just out of reach doesn't count...
  game.world().setBlockAt({5, ground + 1, 15}, BlockType::CRAFTING_TABLE);
  CHECK(reason(run(Craft{kStoneBricksRecipe, false})) == "craft: needs a Crafting Table nearby");
  CHECK(heldCount(inventory(), itemForBlock(BlockType::COBBLESTONE)) == 4);

  // ...one close by does.
  game.world().setBlockAt({5, ground + 1, 6}, BlockType::CRAFTING_TABLE);
  run(Craft{kStoneBricksRecipe, false});
  CHECK(heldCount(inventory(), itemForBlock(BlockType::STONE_BRICKS)) == 4);
  CHECK(heldCount(inventory(), itemForBlock(BlockType::COBBLESTONE)) == 0);
}

TEST_CASE_FIXTURE(EditGame, "missing ingredients, unknown recipes and a full inventory change nothing") {
  CHECK(reason(run(Craft{kTableRecipe, false})) == "craft: you haven't discovered that yet");
  run(GiveItem{itemForBlock(BlockType::PINE_PLANKS), 3});
  CHECK(reason(run(Craft{kTableRecipe, false})) == "craft: missing ingredients");
  CHECK(reason(run(Craft{9999, false})) == "craft: no such recipe");

  // Two logs in one slot (so it doesn't free up) and every other slot full.
  PlayerState &me = *game.findPlayer(id);
  for (int i = 0; i < kInventorySlots; ++i) me.inventory.set(i, {ItemId::COAL, 64});
  me.inventory.set(3, {itemForBlock(BlockType::OAK_LOG), 2});
  see(me.seen, itemForBlock(BlockType::OAK_LOG));
  Inventory before = me.inventory;
  CHECK(reason(run(Craft{kOakPlanksRecipe, false})) == "craft: no room in your inventory");
  CHECK(me.inventory.slots() == before.slots());
}

TEST_CASE_FIXTURE(EditGame, "creative knows every recipe") {
  // Put the ingredients straight in, so neither counts as seen.
  PlayerState &me = *game.findPlayer(id);
  me.inventory.set(20, {itemForBlock(BlockType::WHITE_BLOCK), 8});
  me.inventory.set(21, {ItemId::RUBY, 1});
  game.world().setBlockAt({5, ground + 1, 6}, BlockType::CRAFTING_TABLE);
  CHECK(reason(run(Craft{kRedRecipe, false})) == "craft: you haven't discovered that yet");

  game.setMode(GameMode::Creative);
  run(Craft{kRedRecipe, false});
  CHECK(heldCount(inventory(), itemForBlock(BlockType::RED_BLOCK)) == 8);
}

// ─── Furnaces ──────────────────────────────────────────────────────────

namespace {
void runTicks(Game &game, int ticks) {
  for (int i = 0; i < ticks; ++i) game.tick();
}
int dropped(const Game &game, ItemId id) {
  int n = 0;
  for (const auto &[eid, e] : game.entities().all())
    if (e.kind == EntityKind::DroppedItem && e.item.id == id) n += e.item.count;
  return n;
}
} // namespace

TEST_CASE_FIXTURE(EditGame, "placing a furnace makes one; breaking it spills what's inside") {
  glm::ivec3 at_{5, ground + 1, 5};
  REQUIRE(changes(place(at_, BlockType::FURNACE)) == 1);
  Furnace *f = game.furnaceAt(at_);
  REQUIRE(f);
  f->input = {ItemId::RAW_IRON, 3};
  f->output = {ItemId::IRON_INGOT, 2};
  mine(at_);
  CHECK_FALSE(game.furnaceAt(at_));
  CHECK(dropped(game, ItemId::RAW_IRON) == 3);
  CHECK(dropped(game, ItemId::IRON_INGOT) == 2);
}

TEST_CASE_FIXTURE(EditGame, "a burning furnace glows, and goes dark when the fuel runs out") {
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::FURNACE);
  Furnace *f = game.furnaceAt(at_);
  f->input = {itemForBlock(BlockType::SAND), 1};
  f->fuel = {ItemId::STICKS, 1};
  pollAll(conn);
  runTicks(game, 1);
  CHECK(at(at_) == BlockType::FURNACE_LIT);
  CHECK(game.world().getBlockLightAt(at_ + glm::ivec3(0, 1, 0)) == 12);
  bool told = false;
  for (const Event &e : pollAll(other))
    if (const auto *b = std::get_if<BlockChanged>(&e))
      told |= b->pos == at_ && b->type == BlockType::FURNACE_LIT;
  CHECK(told);

  runTicks(game, kSmeltMs / Game::kTickMs);
  CHECK(at(at_) == BlockType::FURNACE);
  CHECK(game.world().getBlockLightAt(at_ + glm::ivec3(0, 1, 0)) == 0);
  CHECK(game.furnaceAt(at_)->output.empty()); // half a stick isn't a whole item
  // Breaking a lit furnace gives a furnace back.
  game.furnaceAt(at_)->fuel = {ItemId::COAL, 1};
  runTicks(game, 1);
  REQUIRE(at(at_) == BlockType::FURNACE_LIT);
  wield(ItemId::WOOD_PICKAXE);
  mine(at_);
  CHECK(heldCount(inventory(), itemForBlock(BlockType::FURNACE)) == 1);
}

TEST_CASE_FIXTURE(EditGame, "furnaces are saved and keep cooking where nobody is") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string()));
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::FURNACE);
  Furnace *f = game.furnaceAt(at_);
  f->input = {ItemId::RAW_GOLD, 4};
  f->fuel = {ItemId::COAL, 1};
  runTicks(game, 1); // lit
  run(Save{});

  ScopedRenderDistance rd2{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  REQUIRE(loaded.furnaceAt(at_));
  CHECK(loaded.furnaceAt(at_)->lit());
  CHECK(loaded.world().getChunkCount() == 0); // nobody here: nothing loaded
  runTicks(loaded, 5 * kSmeltMs / Game::kTickMs);
  CHECK(loaded.furnaceAt(at_)->output == ItemStack{ItemId::GOLD_INGOT, 4});
  CHECK(loaded.furnaceAt(at_)->input.empty());
  // The coal burns out while still unloaded; the block shows it once loaded.
  runTicks(loaded, 8 * kSmeltMs / Game::kTickMs);
  loaded.world().loadChunkSync(0, 0);
  loaded.world().flushPendingEdits();
  CHECK(loaded.world().getBlockAt(glm::vec3(at_) + 0.5f) == BlockType::FURNACE);
}

namespace {
std::optional<ContainerView> lastView(const std::vector<Event> &evs) {
  std::optional<ContainerView> v;
  for (const Event &e : evs)
    if (const auto *c = std::get_if<ContainerView>(&e)) v = *c;
  return v;
}
} // namespace

TEST_CASE_FIXTURE(EditGame, "opening a furnace shows it; only furnaces in reach open") {
  glm::ivec3 at_{5, ground + 1, 5};
  CHECK(reason(run(OpenContainer{at_})) == "open: that isn't a furnace or a chest");
  place(at_, BlockType::FURNACE);
  game.furnaceAt(at_)->output = {ItemId::IRON_INGOT, 3};
  std::optional<ContainerView> v = lastView(run(OpenContainer{at_}));
  REQUIRE(v);
  CHECK(v->open);
  CHECK(v->pos == at_);
  CHECK(v->output == ItemStack{ItemId::IRON_INGOT, 3});

  game.world().setBlockAt({5, ground + 1, 15}, BlockType::FURNACE);
  CHECK(reason(run(OpenContainer{{5, ground + 1, 15}})) == "open: out of reach");
}

TEST_CASE_FIXTURE(EditGame, "items go in and out of a furnace by the slot rules") {
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::FURNACE);
  PlayerState &me = *game.findPlayer(id);
  me.inventory.set(20, {ItemId::RAW_IRON, 5});
  me.inventory.set(21, {ItemId::COAL, 3});
  me.inventory.set(22, {itemForBlock(BlockType::DIRT), 4});
  run(OpenContainer{at_});

  CHECK(reason(run(MoveItems{20, kFurnaceFuel, 0})) == "move: not allowed");   // ore isn't fuel
  CHECK(reason(run(MoveItems{22, kFurnaceInput, 0})) == "move: not allowed");  // dirt won't smelt
  CHECK(reason(run(MoveItems{21, kFurnaceOutput, 0})) == "move: not allowed"); // nothing goes in there
  run(MoveItems{20, kFurnaceInput, 0});
  run(MoveItems{21, kFurnaceFuel, 1}); // one lump
  Furnace &f = *game.furnaceAt(at_);
  CHECK(f.input == ItemStack{ItemId::RAW_IRON, 5});
  CHECK(f.fuel == ItemStack{ItemId::COAL, 1});
  CHECK(me.inventory.slot(21) == ItemStack{ItemId::COAL, 2});
  CHECK(me.inventory.slot(20).empty());

  f.output = {ItemId::IRON_INGOT, 2};
  auto evs = run(MoveItems{kFurnaceOutput, 25, 0});
  CHECK(me.inventory.slot(25) == ItemStack{ItemId::IRON_INGOT, 2});
  CHECK(f.output.empty());
  REQUIRE(lastView(evs));
  CHECK(lastView(evs)->output.empty());

  // A swap may not put the wrong thing in: dirt can't trade places with the ore.
  CHECK(reason(run(MoveItems{22, kFurnaceInput, 0})) == "move: not allowed");
  CHECK(f.input == ItemStack{ItemId::RAW_IRON, 5});
}

TEST_CASE_FIXTURE(EditGame, "moves need an open furnace, and walking away or breaking it closes it") {
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::FURNACE);
  PlayerState &me = *game.findPlayer(id);
  me.inventory.set(20, {ItemId::RAW_IRON, 1});
  CHECK(reason(run(MoveItems{20, kFurnaceInput, 0})) == "move: nothing open");

  run(OpenContainer{at_});
  moveTo({5.5f, ground + 1.0f, 15.5f}); // out of reach
  game.tick();
  std::optional<ContainerView> v = lastView(pollAll(conn));
  REQUIRE(v);
  CHECK_FALSE(v->open);
  CHECK_FALSE(me.open_container);

  moveTo({5.5f, ground + 1.0f, 8.5f});
  run(OpenContainer{at_});
  mine(at_);
  game.tick();
  v = lastView(pollAll(conn));
  REQUIRE(v);
  CHECK_FALSE(v->open);
}

TEST_CASE_FIXTURE(EditGame, "a viewer sees the furnace cook") {
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::FURNACE);
  Furnace &f = *game.furnaceAt(at_);
  f.input = {itemForBlock(BlockType::SAND), 1};
  f.fuel = {ItemId::COAL, 1};
  run(OpenContainer{at_});
  for (int i = 0; i < kSmeltMs / Game::kTickMs; ++i) game.tick();
  std::optional<ContainerView> v = lastView(pollAll(conn));
  REQUIRE(v);
  CHECK(v->output == ItemStack{itemForBlock(BlockType::GLASS), 1});
  CHECK(v->burn > 0);
}

// ─── Mining with tools (survival) ──────────────────────────────────────

TEST_CASE_FIXTURE(EditGame, "a break before its time is refused, and the block stays") {
  glm::ivec3 rock{5, ground + 1, 5};
  game.world().setBlockAt(rock, BlockType::STONE);
  CHECK(reason(run(BreakBlock{rock})) == "break: not yet"); // never started
  run(StartBreak{rock});
  CHECK(reason(run(BreakBlock{rock})) == "break: not yet"); // started, but just now
  CHECK(at(rock) == BlockType::STONE);

  // Starting on another block starts over.
  run(StartBreak{rock});
  for (int t = 0; t < 7500; t += Game::kTickMs) game.tick();
  run(StartBreak{{6, ground + 1, 5}});
  pollAll(conn);
  CHECK(reason(run(BreakBlock{rock})) == "break: not yet");
}

TEST_CASE_FIXTURE(EditGame, "the time is judged by what's held when it breaks") {
  glm::ivec3 rock{5, ground + 1, 5};
  game.world().setBlockAt(rock, BlockType::STONE);
  wield(ItemId::DIAMOND_PICKAXE);
  run(StartBreak{rock});
  for (int t = 0; t < breakMs(BlockType::STONE, ItemId::DIAMOND_PICKAXE); t += Game::kTickMs)
    game.tick();
  wield(itemForBlock(BlockType::DIRT)); // swapped to something slow at the last moment
  pollAll(conn);
  CHECK(reason(run(BreakBlock{rock})) == "break: not yet");
  wield(ItemId::DIAMOND_PICKAXE);
  CHECK(changes(run(BreakBlock{rock})) == 1);
}

TEST_CASE_FIXTURE(EditGame, "creative breaks anything at once") {
  game.setMode(GameMode::Creative);
  glm::ivec3 ore{5, ground + 1, 5};
  game.world().setBlockAt(ore, BlockType::DIAMOND_ORE);
  CHECK(changes(run(BreakBlock{ore})) == 1);
}

TEST_CASE_FIXTURE(EditGame, "ore won't break without a good enough pickaxe, so it isn't wasted") {
  glm::ivec3 ore{5, ground + 1, 5};
  game.world().setBlockAt(ore, BlockType::IRON_ORE);
  wield(ItemId::STONE_PICKAXE);
  CHECK(changes(mine(ore)) == 0);
  CHECK(game.world().getBlockAt(glm::vec3(ore) + 0.5f) == BlockType::IRON_ORE);
  CHECK(heldCount(inventory(), ItemId::RAW_IRON) == 0);
  wield(ItemId::COPPER_PICKAXE);
  mine(ore);
  CHECK(heldCount(inventory(), ItemId::RAW_IRON) == 1);
}

TEST_CASE_FIXTURE(EditGame, "tools wear with each block and break when used up") {
  PlayerState &me = *game.findPlayer(id);
  glm::ivec3 rock{5, ground + 1, 5};
  game.world().setBlockAt(rock, BlockType::STONE);
  wield(ItemId::WOOD_PICKAXE);
  mine(rock);
  CHECK(me.inventory.slot(me.selected_slot) == ItemStack{ItemId::WOOD_PICKAXE, 1, 1});

  me.inventory.set(me.selected_slot, {ItemId::WOOD_PICKAXE, 1, 59}); // one use left
  game.world().setBlockAt(rock, BlockType::STONE);
  auto evs = mine(rock);
  CHECK(me.inventory.slot(me.selected_slot).empty());
  bool told = false;
  for (const Event &e : evs)
    if (const auto *b = std::get_if<ToolBroke>(&e)) told = b->item == ItemId::WOOD_PICKAXE;
  CHECK(told);
  CHECK(heldCount(inventory(), itemForBlock(BlockType::COBBLESTONE)) == 2); // the last one counted
}

TEST_CASE_FIXTURE(EditGame, "an emerald pickaxe sometimes doubles ore") {
  glm::ivec3 ore{5, ground + 1, 5};
  wield(ItemId::EMERALD_PICKAXE);
  constexpr int kOres = 60;
  for (int i = 0; i < kOres; ++i) {
    game.world().setBlockAt(ore, BlockType::COAL_ORE);
    mine(ore);
  }
  int coal = heldCount(inventory(), ItemId::COAL);
  CHECK(coal > kOres + kOres / 6);  // about a third more...
  CHECK(coal < kOres + kOres / 2);  // ...not half or more
  // Not for blocks that drop themselves.
  wield(ItemId::EMERALD_PICKAXE);
  for (int i = 0; i < 20; ++i) {
    game.world().setBlockAt(ore, BlockType::STONE);
    mine(ore);
  }
  CHECK(heldCount(inventory(), itemForBlock(BlockType::COBBLESTONE)) == 20);
}

TEST_CASE_FIXTURE(SmallGame, "save without a bound directory fails") {
  CHECK_FALSE(game.save());
}

// ─── Survival inventory ────────────────────────────────────────────────

namespace {

// The latest inventory snapshot in a batch of events, if any.
std::optional<Inventory::Slots> lastContents(const std::vector<Event> &evs) {
  std::optional<Inventory::Slots> out;
  for (const Event &e : evs)
    if (const auto *c = std::get_if<InventoryContents>(&e)) out = c->slots;
  return out;
}

int countOf(const Inventory &inv, ItemId id) {
  int n = 0;
  for (const ItemStack &s : inv.slots())
    if (s.id == id) n += s.count;
  return n;
}

} // namespace

TEST_CASE_FIXTURE(SmallGame, "a new player starts with the kit and is told so") {
  LocalConnection conn;
  game.addClient(conn);
  conn.send(Join{});
  game.pump();
  auto evs = pollAll(conn);

  auto contents = lastContents(evs);
  REQUIRE(contents);
  CHECK((*contents)[0] == ItemStack{itemForBlock(BlockType::GLOWSTONE), 8});
  for (int i = 1; i < kInventorySlots; ++i) CHECK((*contents)[i].empty());

  PlayerId id = std::get<Joined>(evs[0]).id;
  CHECK(game.findPlayer(id)->inventory.slots() == *contents);
}

TEST_CASE_FIXTURE(EditGame, "breaking collects the block's drop") {
  glm::ivec3 target{5, ground, 5};
  BlockType was = at(target);
  REQUIRE(dropFor(was));
  ItemId drop = *dropFor(was);

  auto evs = mine(target);
  CHECK(countOf(inventory(), drop) == 1);
  auto contents = lastContents(evs);
  REQUIRE(contents);
  CHECK(*contents == inventory().slots());
  // Only the breaker hears about their inventory.
  auto seen = pollAll(other);
  CHECK_FALSE(lastContents(seen));
}

TEST_CASE_FIXTURE(EditGame, "breaking ore collects its material") {
  glm::ivec3 ore{5, ground + 1, 5};
  game.world().setBlockAt(ore, BlockType::DIAMOND_ORE);
  wield(ItemId::IRON_PICKAXE);
  mine(ore);
  CHECK(countOf(inventory(), ItemId::DIAMOND) == 1);
  CHECK(countOf(inventory(), itemForBlock(BlockType::DIAMOND_ORE)) == 0);
}

TEST_CASE_FIXTURE(EditGame, "with a full inventory the drop pops out of the block") {
  wield(ItemId::WOOD_PICKAXE); // in place of the kit's glowstone
  run(GiveItem{itemForBlock(BlockType::SAND), 64 * kInventorySlots});
  // Every slot but the pickaxe's is now a full stack of sand.
  REQUIRE(countOf(inventory(), itemForBlock(BlockType::SAND)) ==
          64 * (kInventorySlots - 1));
  Inventory before = inventory();
  size_t entities_before = game.entities().size();

  glm::ivec3 target{5, ground + 1, 5};
  game.world().setBlockAt(target, BlockType::COBBLESTONE);
  CHECK(changes(mine(target)) == 1);
  CHECK(at(target) == BlockType::AIR);
  int held = game.findPlayer(id)->selected_slot;
  before.set(held, {ItemId::WOOD_PICKAXE, 1, 1}); // one use spent, nothing else changed
  CHECK(inventory().slots() == before.slots());

  REQUIRE(game.entities().size() == entities_before + 1);
  const Entity *drop = nullptr;
  for (const auto &[id, e] : game.entities().all())
    if (e.item.id == itemForBlock(BlockType::COBBLESTONE)) drop = &e;
  REQUIRE(drop);
  CHECK(drop->kind == EntityKind::DroppedItem);
  CHECK(drop->item.count == 1);
  CHECK(glm::distance(drop->body.position, glm::vec3(target) + 0.5f) < 1.0f);
  CHECK(drop->pickup_delay_ms > 0);
}

TEST_CASE_FIXTURE(EditGame, "placing consumes one of the held block") {
  run(GiveItem{itemForBlock(BlockType::COBBLESTONE), 3});
  int slot = -1;
  for (int i = 0; i < kHotbarSlots; ++i)
    if (inventory().slot(i).id == itemForBlock(BlockType::COBBLESTONE)) slot = i;
  REQUIRE(slot >= 0);
  run(SelectSlot{slot});

  auto evs = run(PlaceBlock{{5, ground + 1, 5}});
  CHECK(changes(evs) == 1);
  CHECK(at({5, ground + 1, 5}) == BlockType::COBBLESTONE);
  CHECK(inventory().slot(slot) == ItemStack{itemForBlock(BlockType::COBBLESTONE), 2});
  REQUIRE(lastContents(evs));
  CHECK(*lastContents(evs) == inventory().slots());

  run(PlaceBlock{{6, ground + 1, 5}});
  run(PlaceBlock{{7, ground + 1, 5}});
  CHECK(inventory().slot(slot).empty());
  CHECK(rejected(run(PlaceBlock{{8, ground + 1, 5}})));
  CHECK(at({8, ground + 1, 5}) == BlockType::AIR);
}

TEST_CASE_FIXTURE(EditGame, "the starter glowstone can be placed") {
  run(SelectSlot{0});
  CHECK(changes(run(PlaceBlock{{5, ground + 2, 5}})) == 1);
  CHECK(at({5, ground + 2, 5}) == BlockType::GLOWSTONE);
  CHECK(inventory().slot(0).count == 7);
}

TEST_CASE_FIXTURE(EditGame, "a held non-block item can't be placed") {
  run(GiveItem{ItemId::RUBY, 1});
  int slot = -1;
  for (int i = 0; i < kHotbarSlots; ++i)
    if (inventory().slot(i).id == ItemId::RUBY) slot = i;
  REQUIRE(slot >= 0);
  run(SelectSlot{slot});

  CHECK(rejected(run(PlaceBlock{{5, ground + 1, 5}})));
  CHECK(at({5, ground + 1, 5}) == BlockType::AIR);
  CHECK(countOf(inventory(), ItemId::RUBY) == 1);
}

TEST_CASE_FIXTURE(EditGame, "a rejected place keeps the item") {
  run(SelectSlot{0});
  CHECK(rejected(run(PlaceBlock{{5, ground, 5}}))); // occupied
  CHECK(inventory().slot(0).count == 8);
}

TEST_CASE_FIXTURE(EditGame, "GiveItem adds valid items and rejects unknown ones") {
  auto evs = run(GiveItem{ItemId::EMERALD, 5});
  CHECK(countOf(inventory(), ItemId::EMERALD) == 5);
  CHECK(lastContents(evs));

  CHECK(rejected(run(GiveItem{static_cast<ItemId>(9999), 1})));
  CHECK(rejected(run(GiveItem{ItemId::EMERALD, 0})));
}

TEST_CASE_FIXTURE(EditGame, "MoveItems rearranges the inventory") {
  run(GiveItem{ItemId::COAL, 10});
  REQUIRE(inventory().slot(1) == ItemStack{ItemId::COAL, 10});

  auto evs = run(MoveItems{1, 30, 4});
  CHECK(inventory().slot(1) == ItemStack{ItemId::COAL, 6});
  CHECK(inventory().slot(30) == ItemStack{ItemId::COAL, 4});
  CHECK(lastContents(evs));

  run(MoveItems{0, 1, 0}); // whole glowstone stack onto coal: swap
  CHECK(inventory().slot(0) == ItemStack{ItemId::COAL, 6});
  CHECK(inventory().slot(1) == ItemStack{itemForBlock(BlockType::GLOWSTONE), 8});

  CHECK(rejected(run(MoveItems{0, 1, 2})));  // partial onto a different item
  CHECK(rejected(run(MoveItems{5, 6, 0})));  // empty source
  CHECK(rejected(run(MoveItems{0, 99, 0}))); // out of range
}

TEST_CASE_FIXTURE(SmallGame, "an old save without inventory.dat gets the starter kit") {
  ScopedSeed seed;
  TempDir dir;
  WorldSave::LevelData level;
  WorldSave::PlayerData pd;
  pd.position = {3.5f, 90.0f, 3.5f};
  REQUIRE(WorldSave::save(dir.path.string(), level, pd, {}));

  REQUIRE(game.load(dir.path.string()));
  LocalConnection conn;
  Joined j = join(game, conn);
  CHECK(j.position == pd.position);
  CHECK(game.findPlayer(j.id)->inventory.slots() == Game::starterKit().slots());

  // Saving now writes inventory.dat, so the kit isn't handed out again.
  REQUIRE(game.save());
  std::vector<ItemStack> on_disk;
  CHECK(WorldSave::loadInventory(dir.path.string(), on_disk));
}

TEST_CASE_FIXTURE(SmallGame, "only the first join takes the saved inventory") {
  std::vector<ItemStack> saved(kInventorySlots);
  saved[4] = {ItemId::RUBY, 2};
  game.restore({}, {}, std::nullopt, saved);

  LocalConnection a, b;
  Joined ja = join(game, a);
  Joined jb = join(game, b);
  CHECK(game.findPlayer(ja.id)->inventory.slot(4) == ItemStack{ItemId::RUBY, 2});
  CHECK(game.findPlayer(jb.id)->inventory.slots() == Game::starterKit().slots());
}

// ─── Vitals ────────────────────────────────────────────────────────────

namespace {

std::optional<VitalsChanged> lastVitals(const std::vector<Event> &evs) {
  std::optional<VitalsChanged> out;
  for (const Event &e : evs)
    if (const auto *v = std::get_if<VitalsChanged>(&e)) out = *v;
  return out;
}

MovePlayer at(glm::vec3 pos, bool on_ground) {
  MovePlayer m;
  m.position = pos;
  m.on_ground = on_ground;
  return m;
}

// Joined player plus an observer, for vitals tests.
struct VitalsGame : SmallGame {
  LocalConnection conn, other;
  PlayerId id = 0;

  VitalsGame() {
    id = join(game, conn).id;
    join(game, other);
  }

  const PlayerState &me() const { return *game.findPlayer(id); }

  std::vector<Event> send(Command cmd) {
    conn.send(std::move(cmd));
    game.pump();
    return pollAll(conn);
  }

  // Falls from `height` blocks above y=100 onto y=100.
  std::vector<Event> fall(float height) {
    send(at({0.5f, 100.0f + height, 0.5f}, true));
    send(at({0.5f, 100.0f + height - 0.1f, 0.5f}, false));
    return send(at({0.5f, 100.0f, 0.5f}, true));
  }

  std::vector<Event> advance(float seconds) {
    game.advance(seconds);
    return pollAll(conn);
  }
};

} // namespace

TEST_CASE_FIXTURE(VitalsGame, "joining sends full vitals") {
  LocalConnection c;
  game.addClient(c);
  c.send(Join{});
  game.pump();
  auto v = lastVitals(pollAll(c));
  REQUIRE(v);
  CHECK(*v == VitalsChanged{20, 20, 20, true, false});
}

TEST_CASE_FIXTURE(VitalsGame, "a long fall hurts its owner, who alone is told") {
  pollAll(other);
  auto evs = fall(10.0f);
  CHECK(me().vitals.health() == 13);
  auto v = lastVitals(evs);
  REQUIRE(v);
  CHECK(v->health == 13);
  CHECK_FALSE(lastVitals(pollAll(other)));
}

TEST_CASE_FIXTURE(VitalsGame, "a short fall doesn't hurt and sends nothing") {
  auto evs = fall(3.0f);
  CHECK(me().vitals.health() == 20);
  CHECK_FALSE(lastVitals(evs));
}

TEST_CASE_FIXTURE(VitalsGame, "fly mode and water make falls safe") {
  MovePlayer up = at({0.5f, 150.0f, 0.5f}, false);
  up.fly = true;
  send(up);
  send(at({0.5f, 149.0f, 0.5f}, false)); // stopped flying
  MovePlayer splash = at({0.5f, 100.0f, 0.5f}, false);
  splash.in_water = true;
  send(splash);
  send(at({0.5f, 100.0f, 0.5f}, true));
  CHECK(me().vitals.health() == 20);
}

TEST_CASE_FIXTURE(VitalsGame, "staying underwater drowns over ticks") {
  MovePlayer under = at({0.5f, 60.0f, 0.5f}, false);
  under.in_water = under.eyes_underwater = true;
  send(under);

  auto evs = advance(5.0f);
  CHECK(me().vitals.breath() == 10);
  REQUIRE(lastVitals(evs));
  CHECK(lastVitals(evs)->breath == 10);

  advance(6.0f);
  CHECK(me().vitals.breath() == 0);
  CHECK(me().vitals.health() == 18);
}

TEST_CASE_FIXTURE(VitalsGame, "health regenerates over ticks") {
  fall(8.0f); // 5 damage
  CHECK(me().vitals.health() == 15);
  advance(6.0f);
  CHECK(me().vitals.health() == 16);
}

TEST_CASE_FIXTURE(VitalsGame, "sprinting until exhausted tells the client to stop") {
  MovePlayer run = at({0.5f, 70.0f, 0.5f}, true);
  run.sprinting = true;
  send(run);
  auto evs = advance(10.0f);
  auto v = lastVitals(evs);
  REQUIRE(v);
  CHECK(v->stamina == 0);
  CHECK_FALSE(v->can_sprint);
}

TEST_CASE_FIXTURE(VitalsGame, "death freezes the player, then respawns them with their items") {
  send(GiveItem{ItemId::DIAMOND, 4});
  Inventory before = me().inventory;

  auto evs = fall(40.0f);
  REQUIRE(me().vitals.dead());
  auto v = lastVitals(evs);
  REQUIRE(v);
  CHECK(v->dead);
  CHECK(v->health == 0);

  // Dead: movement and edits are ignored, without rejections.
  glm::vec3 where = me().position;
  CHECK(send(at({50.5f, 90.0f, 50.5f}, true)).empty());
  CHECK(me().position == where);
  CHECK(send(BreakBlock{{0, 99, 0}}).empty());
  CHECK(send(PlaceBlock{{0, 101, 0}}).empty());

  advance(2.9f);
  CHECK(me().vitals.dead());

  evs = advance(0.2f);
  CHECK_FALSE(me().vitals.dead());
  bool respawned = false;
  for (const Event &e : evs)
    if (const auto *r = std::get_if<Respawned>(&e)) {
      respawned = true;
      CHECK(r->position == game.world().findSpawn());
    }
  CHECK(respawned);
  REQUIRE(lastVitals(evs));
  CHECK(*lastVitals(evs) == VitalsChanged{20, 20, 20, true, false});
  CHECK(me().position == game.world().findSpawn());
  CHECK(me().inventory.slots() == before.slots());
}

TEST_CASE_FIXTURE(VitalsGame, "a fall in progress at respawn doesn't count") {
  fall(40.0f);
  advance(3.1f);
  // The client was mid-air when it died; its next report lands at spawn.
  glm::vec3 spawn = game.world().findSpawn();
  send(at(spawn, true));
  CHECK(me().vitals.health() == 20);
}

// ─── Entity snapshots ──────────────────────────────────────────────────

namespace {

std::optional<EntitySnapshot> lastSnapshot(const std::vector<Event> &evs) {
  std::optional<EntitySnapshot> out;
  for (const Event &e : evs)
    if (const auto *s = std::get_if<EntitySnapshot>(&e)) out = *s;
  return out;
}

} // namespace

TEST_CASE_FIXTURE(VitalsGame, "every tick each player gets the entities near them") {
  glm::vec3 me_at = me().position;
  auto near = game.entities().spawn(EntityKind::TestCreature, me_at + glm::vec3(5, 0, 0));
  auto far = game.entities().spawn(EntityKind::TestCreature, me_at + glm::vec3(0, 0, 90));
  REQUIRE(near);
  REQUIRE(far);
  // The observer stands right next to the far creature.
  other.send(at(me_at + glm::vec3(0, 0, 89), true));
  game.pump();
  pollAll(other);

  auto evs = advance(0.05f);
  auto snap = lastSnapshot(evs);
  REQUIRE(snap);
  bool saw_near = false, saw_far = false;
  for (const EntityView &v : snap->entities) {
    saw_near |= v.id == *near;
    saw_far |= v.id == *far;
  }
  CHECK(saw_near);
  CHECK(saw_far); // 90 blocks: inside the 96-block range

  // Move the far one out of my range but keep it within the observer's.
  game.entities().find(*far)->body.position = me_at + glm::vec3(0, 0, 100);
  snap = lastSnapshot(advance(0.05f));
  REQUIRE(snap);
  for (const EntityView &v : snap->entities) CHECK(v.id != *far);
  auto theirs = lastSnapshot(pollAll(other));
  REQUIRE(theirs);
  bool they_see_far = false;
  for (const EntityView &v : theirs->entities) they_see_far |= v.id == *far;
  CHECK(they_see_far);
}

TEST_CASE_FIXTURE(VitalsGame, "snapshots carry what the client needs to draw") {
  auto id = game.entities().spawn(EntityKind::DroppedItem, me().position);
  REQUIRE(id);
  game.entities().find(*id)->item = {ItemId::RUBY, 3};
  game.entities().find(*id)->pickup_delay_ms = 10000; // don't pick it up
  auto snap = lastSnapshot(advance(0.05f));
  REQUIRE(snap);
  REQUIRE(snap->entities.size() == 1);
  CHECK(snap->entities[0].kind == EntityKind::DroppedItem);
  CHECK(snap->entities[0].item == ItemId::RUBY);
  CHECK(snap->tick > 0);
}

// ─── Dropped items ─────────────────────────────────────────────────────

namespace {

std::vector<const Entity *> droppedItems(const Game &g) {
  std::vector<const Entity *> out;
  for (const auto &[id, e] : g.entities().all())
    if (e.kind == EntityKind::DroppedItem) out.push_back(&e);
  return out;
}

} // namespace

TEST_CASE_FIXTURE(EditGame, "Q throws one of the held item; it can't be grabbed straight back") {
  const ItemId glow = itemForBlock(BlockType::GLOWSTONE);
  run(SelectSlot{0});
  run(DropItem{false});
  CHECK(inventory().slot(0) == ItemStack{glow, 7});
  auto drops = droppedItems(game);
  REQUIRE(drops.size() == 1);
  EntityId id = drops[0]->id;
  CHECK(drops[0]->item == ItemStack{glow, 1});

  // Hold it right at the player for most of the delay: still on the ground.
  glm::vec3 centre = game.findPlayer(this->id)->position + glm::vec3(0, 0.9f, 0);
  for (int i = 0; i < 25; ++i) { // 1.25 s
    game.entities().find(id)->body.position = centre;
    game.advance(0.05f);
  }
  REQUIRE(game.entities().find(id));
  CHECK(inventory().slot(0).count == 7);

  for (int i = 0; i < 6; ++i) { // past 1.5 s
    if (Entity *e = game.entities().find(id)) e->body.position = centre;
    game.advance(0.05f);
  }
  CHECK_FALSE(game.entities().find(id));
  CHECK(inventory().slot(0).count == 8);
}

TEST_CASE_FIXTURE(EditGame, "Ctrl+Q throws the whole stack; an empty hand throws nothing") {
  run(SelectSlot{0});
  run(DropItem{true});
  CHECK(inventory().slot(0).empty());
  auto drops = droppedItems(game);
  REQUIRE(drops.size() == 1);
  CHECK(drops[0]->item.count == 8);

  CHECK(rejected(run(DropItem{false})));
  CHECK(droppedItems(game).size() == 1);
}

TEST_CASE_FIXTURE(EditGame, "picking up takes what fits and leaves the rest") {
  run(GiveItem{itemForBlock(BlockType::SAND), 64 * (kInventorySlots - 1)});
  REQUIRE(inventory().slot(0).count == 8); // glowstone: room for 56 more
  glm::vec3 centre = game.findPlayer(id)->position + glm::vec3(0, 0.9f, 0);
  auto drop = game.entities().spawn(EntityKind::DroppedItem, centre);
  REQUIRE(drop);
  game.entities().find(*drop)->item = {itemForBlock(BlockType::GLOWSTONE), 60};
  game.entities().find(*drop)->pickup_delay_ms = 0;

  auto evs = [&] { game.advance(0.05f); return pollAll(conn); }();
  CHECK(inventory().slot(0).count == 64);
  REQUIRE(game.entities().find(*drop));
  CHECK(game.entities().find(*drop)->item.count == 4);
  CHECK(lastContents(evs));
}

TEST_CASE_FIXTURE(EditGame, "identical items lying together merge") {
  glm::vec3 spot{10.5f, ground + 1.0f, 2.5f}; // well away from the player
  auto a = game.entities().spawn(EntityKind::DroppedItem, spot);
  auto b = game.entities().spawn(EntityKind::DroppedItem, spot + glm::vec3(0.5f, 0, 0));
  auto c = game.entities().spawn(EntityKind::DroppedItem, spot + glm::vec3(0, 0, 0.4f));
  game.entities().find(*a)->item = {ItemId::COAL, 3};
  game.entities().find(*b)->item = {ItemId::COAL, 4};
  game.entities().find(*c)->item = {ItemId::RUBY, 1}; // different item: stays
  game.advance(0.05f);

  CHECK(game.entities().find(*a)->item == ItemStack{ItemId::COAL, 7});
  CHECK_FALSE(game.entities().find(*b));
  CHECK(game.entities().find(*c));
}

TEST_CASE_FIXTURE(EditGame, "merging stops at the max stack") {
  glm::vec3 spot{10.5f, ground + 1.0f, 2.5f};
  auto a = game.entities().spawn(EntityKind::DroppedItem, spot);
  auto b = game.entities().spawn(EntityKind::DroppedItem, spot + glm::vec3(0.3f, 0, 0));
  game.entities().find(*a)->item = {ItemId::COAL, 60};
  game.entities().find(*b)->item = {ItemId::COAL, 10};
  game.advance(0.05f);
  CHECK(game.entities().find(*a)->item.count == 64);
  REQUIRE(game.entities().find(*b));
  CHECK(game.entities().find(*b)->item.count == 6);
}

TEST_CASE_FIXTURE(EditGame, "give overflow lands at your feet") {
  run(GiveItem{itemForBlock(BlockType::SAND), 64 * (kInventorySlots - 1)});
  run(GiveItem{itemForBlock(BlockType::GLOWSTONE), 56}); // inventory now full
  run(GiveItem{ItemId::DIAMOND, 5});
  auto drops = droppedItems(game);
  REQUIRE(drops.size() == 1);
  CHECK(drops[0]->item == ItemStack{ItemId::DIAMOND, 5});
  CHECK(glm::distance(drops[0]->body.position, game.findPlayer(id)->position) < 1.0f);
}

// ─── Hitting entities ──────────────────────────────────────────────────

namespace {

// EditGame plus a test creature two blocks in front (-Z) of the player.
struct FightGame : EditGame {
  EntityId target = 0;
  glm::vec3 spot;

  FightGame() {
    // A flat stone strip with clear air above, under the player and the
    // creature, so terrain can't get in the way. Only changed cells are
    // written: every setBlockAt relights the chunk.
    auto put = [&](int x, int y, int z, BlockType b) {
      if (at({x, y, z}) != b) game.world().setBlockAt({x, y, z}, b);
    };
    for (int x = 4; x <= 7; ++x)
      for (int z = 1; z <= 10; ++z) {
        put(x, ground, z, BlockType::STONE);
        for (int y = ground + 1; y <= ground + 3; ++y) put(x, y, z, BlockType::AIR);
      }
    spot = game.findPlayer(id)->position + glm::vec3(0, 0, -2);
    auto t = game.entities().spawn(EntityKind::TestCreature, spot);
    REQUIRE(t);
    target = *t;
  }

  Entity *creature() { return game.entities().find(target); }

  // Wait out the cooldown, putting the creature back in place.
  void wait(float seconds) {
    game.advance(seconds);
    pollAll(conn);
    if (Entity *e = creature()) {
      e->body.position = spot;
      e->body.velocity = glm::vec3(0.0f);
    }
  }
};

} // namespace

TEST_CASE_FIXTURE(FightGame, "a hit hurts, flashes and knocks the creature away") {
  int max = entityType(EntityKind::TestCreature).max_health;
  CHECK(run(AttackEntity{target}).empty());
  CHECK(creature()->health == max - attackDamage(ItemId::NONE));
  CHECK(creature()->hurt_ms > 0);
  CHECK(creature()->body.velocity.z < -1.0f); // away from the player (-Z)
  CHECK(creature()->body.velocity.y > 0.0f);
}

TEST_CASE_FIXTURE(FightGame, "a sword hits harder than a fist, and wears with each hit") {
  int max = entityType(EntityKind::TestCreature).max_health;
  wield(ItemId::IRON_SWORD);
  run(AttackEntity{target});
  CHECK(creature()->health == max - 6);
  PlayerState &me = *game.findPlayer(id);
  CHECK(me.inventory.slot(me.selected_slot).wear == 1);
}

TEST_CASE_FIXTURE(FightGame, "a ruby sword sets the enemy alight") {
  const int max = 30; // tougher than usual, to watch it burn
  creature()->health = max;
  wield(ItemId::RUBY_SWORD);
  run(AttackEntity{target});
  CHECK(creature()->health == max - 7);
  CHECK(creature()->burning_ms > 0);
  for (int i = 0; i < 3000 / Game::kTickMs; ++i) game.tick();
  REQUIRE(creature());
  CHECK(creature()->health == max - 7 - 3); // a point a second for three seconds
  CHECK(creature()->burning_ms <= 0);
  for (int i = 0; i < 2000 / Game::kTickMs; ++i) game.tick();
  CHECK(creature()->health == max - 10); // and then it's out
}

TEST_CASE_FIXTURE(FightGame, "fire can finish an enemy off") {
  wield(ItemId::RUBY_SWORD);
  creature()->health = 8;
  run(AttackEntity{target}); // 7: one left
  REQUIRE(creature());
  for (int i = 0; i < 1000 / Game::kTickMs; ++i) game.tick();
  CHECK_FALSE(creature());
}

TEST_CASE_FIXTURE(FightGame, "creative hits don't wear the sword") {
  game.setMode(GameMode::Creative);
  wield(ItemId::WOOD_SWORD);
  run(AttackEntity{target});
  PlayerState &me = *game.findPlayer(id);
  CHECK(me.inventory.slot(me.selected_slot).wear == 0);
}

TEST_CASE_FIXTURE(FightGame, "hits have a cooldown") {
  int max = entityType(EntityKind::TestCreature).max_health;
  run(AttackEntity{target});
  CHECK(rejected(run(AttackEntity{target})));
  int hit = attackDamage(ItemId::NONE);
  CHECK(creature()->health == max - hit);
  wait(0.4f);
  run(AttackEntity{target});
  CHECK(creature()->health == max - 2 * hit);
}

TEST_CASE_FIXTURE(FightGame, "creatures out of reach can't be hit") {
  creature()->body.position = spot + glm::vec3(0, 0, -6);
  CHECK(rejected(run(AttackEntity{target})));
  CHECK(creature()->health == entityType(EntityKind::TestCreature).max_health);
}

TEST_CASE_FIXTURE(FightGame, "dropped items and unknown ids can't be hit") {
  auto item = game.entities().spawn(EntityKind::DroppedItem, spot);
  game.entities().find(*item)->item = {ItemId::COAL, 1};
  CHECK(rejected(run(AttackEntity{*item})));
  CHECK(rejected(run(AttackEntity{999999})));
}

TEST_CASE_FIXTURE(FightGame, "a knocked-back creature doesn't steer until the flash ends") {
  run(AttackEntity{target});
  game.advance(0.05f);
  CHECK(creature()->body.velocity.z < -1.0f); // still flying back
}

TEST_CASE_FIXTURE(FightGame, "the dead can't attack") {
  // Fall to death first.
  // ::at is the MovePlayer helper (EditGame::at reads blocks).
  glm::vec3 p = game.findPlayer(id)->position;
  conn.send(::at(p + glm::vec3(0, 40, 0), true));
  conn.send(::at(p + glm::vec3(0, 39, 0), false));
  conn.send(::at(p, true));
  game.pump();
  REQUIRE(game.findPlayer(id)->vitals.dead());
  pollAll(conn);
  CHECK(run(AttackEntity{target}).empty());
  CHECK(creature()->health == entityType(EntityKind::TestCreature).max_health);
}

TEST_CASE_FIXTURE(FightGame, "killing a creature removes it and drops its loot") {
  int hits = 0;
  while (creature() && hits < 20) {
    run(AttackEntity{target});
    ++hits;
    wait(0.4f);
  }
  CHECK_FALSE(creature());
  CHECK(hits == entityType(EntityKind::TestCreature).max_health / attackDamage(ItemId::NONE));
  bool loot = false;
  for (const auto &[eid, e] : game.entities().all())
    loot |= e.kind == EntityKind::DroppedItem && e.item.id == ItemId::COAL;
  // The coal may already have been picked up by the player standing nearby.
  CHECK((loot || countOf(inventory(), ItemId::COAL) == 1));
}

TEST_CASE_FIXTURE(FightGame, "killing enemies drops glowstone, and coal about half the time") {
  game.entities().remove(target);
  int coal = 0, glowstone = 0;
  const int kills = 20;
  for (int i = 0; i < kills; ++i) {
    auto e = game.entities().spawn(EntityKind::CaveGrumble, spot);
    REQUIRE(e);
    target = *e;
    for (int hits = 0; creature() && hits < 20; ++hits) {
      run(AttackEntity{target});
      wait(0.4f);
    }
    REQUIRE_FALSE(creature());
    // Count the loot wherever it went (the player stands close enough to
    // pick it up), then clear it for the next kill.
    for (const auto &[eid, d] : game.entities().all()) {
      if (d.item.id == ItemId::COAL) coal += d.item.count;
      if (d.item.id == itemForBlock(BlockType::GLOWSTONE)) glowstone += d.item.count;
    }
    std::vector<EntityId> items;
    for (const auto &[eid, d] : game.entities().all()) items.push_back(eid);
    for (EntityId eid : items) game.entities().remove(eid);
  }
  coal += countOf(inventory(), ItemId::COAL);
  glowstone += countOf(inventory(), itemForBlock(BlockType::GLOWSTONE)) -
               countOf(Game::starterKit(), itemForBlock(BlockType::GLOWSTONE));
  CHECK(glowstone == 2 * kills);
  CHECK(coal > 0);
  CHECK(coal < kills);
}

TEST_CASE_FIXTURE(EditGame, "the debug panel can spawn a test creature in front of you") {
  conn.send(MovePlayer{game.findPlayer(id)->position, -90.0f, 0.0f}); // facing -Z
  game.pump();
  CHECK(run(SpawnDebugEntity{EntityKind::TestCreature}).empty());
  REQUIRE(game.entities().size() == 1);
  const Entity &e = game.entities().all().begin()->second;
  CHECK(e.kind == EntityKind::TestCreature);
  glm::vec3 d = e.body.position - game.findPlayer(id)->position;
  CHECK(d.z == doctest::Approx(-3.0f).epsilon(0.01));
  CHECK(rejected(run(SpawnDebugEntity{static_cast<EntityKind>(999)})));
}

// ─── Animals: feeding and eating ───────────────────────────────────────

namespace {

// FightGame's flat strip, with a woolly where the test creature stood.
struct FeedGame : FightGame {
  EntityId woolly = 0;
  FeedGame() {
    game.entities().remove(target);
    auto w = game.entities().spawn(EntityKind::Woolly, spot);
    REQUIRE(w);
    woolly = *w;
  }
  Entity *animal() { return game.entities().find(woolly); }
  int held() { return inventory().slot(game.findPlayer(id)->selected_slot).count; }
};

} // namespace

TEST_CASE("the natural spawner has a rule per animal and enemy") {
  Game g;
  CHECK(g.spawner().rules().size() ==
        std::size(kAnimalKinds) + std::size(kEnemyKinds) + std::size(kFishKinds));
}

TEST_CASE_FIXTURE(FeedGame, "feeding an animal its favourite makes it happy and it gives back") {
  hold(BlockType::GRASS);
  run(GiveItem{itemForBlock(BlockType::GRASS), 2}); // 3 grass in hand
  REQUIRE(held() == 3);

  auto evs = run(InteractEntity{woolly});
  CHECK_FALSE(rejected(evs));
  CHECK(held() == 2);
  CHECK(animal()->happy_ms > 0);
  CHECK(animal()->fed_cooldown_ms > 0);

  // The product pops out after a second (and the player, close by, may grab it).
  wait(0.9f);
  bool any = false;
  for (const auto &[eid, e] : game.entities().all()) any |= e.item.id == ItemId::WOOL;
  CHECK_FALSE(any);
  game.advance(0.2f);
  any = countOf(inventory(), ItemId::WOOL) > 0;
  for (const auto &[eid, e] : game.entities().all()) any |= e.item.id == ItemId::WOOL;
  CHECK(any);
}

TEST_CASE_FIXTURE(FeedGame, "a fed animal isn't hungry for a while") {
  hold(BlockType::GRASS);
  run(GiveItem{itemForBlock(BlockType::GRASS), 1});
  run(InteractEntity{woolly});
  CHECK(rejected(run(InteractEntity{woolly})));
  CHECK(held() == 1); // the refused grass wasn't eaten
}

TEST_CASE_FIXTURE(FeedGame, "animals refuse the wrong food or an empty hand") {
  hold(BlockType::STONE);
  CHECK(rejected(run(InteractEntity{woolly})));
  CHECK(held() == 1);
  run(SelectSlot{9}); // empty
  CHECK(rejected(run(InteractEntity{woolly})));
  CHECK(animal()->fed_cooldown_ms == 0);
}

TEST_CASE_FIXTURE(FeedGame, "feeding needs reach and an animal") {
  hold(BlockType::GRASS);
  animal()->body.position = spot + glm::vec3(0, 0, -6);
  CHECK(rejected(run(InteractEntity{woolly})));
  CHECK(held() == 1);

  auto critter = game.entities().spawn(EntityKind::TestCreature, spot);
  CHECK(rejected(run(InteractEntity{*critter})));
  CHECK(rejected(run(InteractEntity{424242})));
}

TEST_CASE_FIXTURE(FeedGame, "animals can't be hurt") {
  CHECK(rejected(run(AttackEntity{woolly})));
  REQUIRE(animal());
}

TEST_CASE_FIXTURE(FeedGame, "snapshots show a happy animal") {
  hold(BlockType::GRASS);
  run(InteractEntity{woolly});
  game.advance(0.05f);
  auto snap = lastSnapshot(pollAll(conn));
  REQUIRE(snap);
  bool happy = false;
  for (const EntityView &v : snap->entities)
    if (v.id == woolly) happy = v.happy;
  CHECK(happy);
}

TEST_CASE_FIXTURE(EditGame, "eating food heals and uses one up") {
  // Hurt the player with a fall first.
  glm::vec3 p = game.findPlayer(id)->position;
  conn.send(::at(p + glm::vec3(0, 12, 0), true));
  conn.send(::at(p + glm::vec3(0, 11, 0), false));
  conn.send(::at(p, true));
  game.pump();
  REQUIRE(game.findPlayer(id)->vitals.health() == 11);

  run(GiveItem{ItemId::BERRIES, 3});
  for (int i = 0; i < kHotbarSlots; ++i)
    if (inventory().slot(i).id == ItemId::BERRIES) run(SelectSlot{i});
  CHECK_FALSE(rejected(run(EatItem{})));
  CHECK(game.findPlayer(id)->vitals.health() == 15);
  CHECK(countOf(inventory(), ItemId::BERRIES) == 2);

  run(EatItem{});
  run(EatItem{}); // 19 → 20: capped
  CHECK(game.findPlayer(id)->vitals.health() == 20);
  CHECK(countOf(inventory(), ItemId::BERRIES) == 0);
}

TEST_CASE_FIXTURE(EditGame, "you can't eat at full health or eat non-food") {
  run(GiveItem{ItemId::SWEET_PETALS, 1});
  for (int i = 0; i < kHotbarSlots; ++i)
    if (inventory().slot(i).id == ItemId::SWEET_PETALS) run(SelectSlot{i});
  CHECK(rejected(run(EatItem{})));
  CHECK(countOf(inventory(), ItemId::SWEET_PETALS) == 1);

  run(SelectSlot{0}); // glowstone
  CHECK(rejected(run(EatItem{})));
}

// ─── Enemies ───────────────────────────────────────────────────────────

namespace {

constexpr float kMidnightSeconds = 0.0f, kNoonSeconds = 43200.0f;

// FightGame's strip opened up to the sky, at midnight, on Normal, with an
// enemy kept pressed against the player.
struct EnemyGame : FightGame {
  ScopedDifficulty difficulty{Difficulty::Normal};
  EntityId enemy = 0;
  glm::vec3 me_pos;

  EnemyGame() {
    game.entities().remove(target);
    fill(game.world(), {4, ground + 1, 1}, {7, kChunkHeight - 1, 10}, BlockType::AIR);
    run(SetTimeOfDay{kMidnightSeconds});
    me_pos = game.findPlayer(id)->position;
    enemy = spawnEnemy(EntityKind::CaveGrumble);
  }

  EntityId spawnEnemy(EntityKind k, glm::vec3 offset = {0.0f, 0.0f, -0.8f}) {
    auto e = game.entities().spawn(k, me_pos + offset);
    REQUIRE(e);
    return *e;
  }
  Entity *foe() { return game.entities().find(enemy); }
  int health() const { return game.findPlayer(id)->vitals.health(); }

  // One tick with everyone held in place (the client would be moving the
  // player; here they stand still).
  std::vector<Event> tick() {
    conn.send(MovePlayer{me_pos, 0.0f, 0.0f, false, false, true});
    game.pump();
    if (Entity *e = foe()) {
      e->body.position = me_pos + glm::vec3(0.0f, 0.0f, -0.8f);
      e->body.velocity = glm::vec3(0.0f);
    }
    game.advance(Game::kTickSeconds);
    return pollAll(conn);
  }

  static std::vector<KnockedBack> knocks(const std::vector<Event> &evs) {
    std::vector<KnockedBack> out;
    for (const Event &e : evs)
      if (auto *k = std::get_if<KnockedBack>(&e)) out.push_back(*k);
    return out;
  }
};

} // namespace

TEST_CASE_FIXTURE(EnemyGame, "an enemy touching you hurts and knocks you back") {
  auto evs = tick();
  CHECK(health() == Vitals::kMax - 2);
  auto k = knocks(evs);
  REQUIRE(k.size() == 1);
  CHECK(k[0].velocity.z > 1.0f); // away from the enemy, which is at -Z
  CHECK(k[0].velocity.y > 0.0f);
  CHECK(knocks(pollAll(other)).empty()); // only the player who was hit
}

TEST_CASE_FIXTURE(EnemyGame, "each enemy hits at most once a second") {
  tick();
  int hits = 1;
  for (int i = 0; i < 19; ++i) hits += static_cast<int>(knocks(tick()).size());
  CHECK(hits == 1);
  CHECK(health() == Vitals::kMax - 2);
  hits += static_cast<int>(knocks(tick()).size()); // 1 s after the first
  CHECK(hits == 2);
  CHECK(health() == Vitals::kMax - 4);
}

TEST_CASE_FIXTURE(EnemyGame, "two enemies each hit") {
  spawnEnemy(EntityKind::CaveGrumble, {0.8f, 0.0f, 0.0f});
  // The second one isn't held in place, so hit on the very first tick.
  tick();
  CHECK(health() == Vitals::kMax - 4);
}

TEST_CASE_FIXTURE(EnemyGame, "enemies further than the contact range don't hit") {
  game.entities().remove(enemy);
  enemy = 0;
  auto far = spawnEnemy(EntityKind::CaveGrumble, {0.0f, 0.0f, -3.0f});
  game.entities().find(far)->brain.reset(); // stays put
  for (int i = 0; i < 10; ++i) tick();
  CHECK(health() == Vitals::kMax);
}

TEST_CASE_FIXTURE(EnemyGame, "easy halves the damage, but never below 1") {
  g_settings.difficulty = Difficulty::Easy;
  tick();
  CHECK(health() == Vitals::kMax - 1); // a Grumble's 2 halved

  game.entities().remove(enemy);
  enemy = spawnEnemy(EntityKind::Gloomling);
  for (int i = 0; i < 20; ++i) tick();
  CHECK(health() == Vitals::kMax - 2); // a Gloomling's 1 stays 1
}

TEST_CASE_FIXTURE(EnemyGame, "peaceful removes enemies and spawns none") {
  spawnEnemy(EntityKind::Gloomling, {3.0f, 0.0f, 0.0f});
  g_settings.difficulty = Difficulty::Peaceful;
  tick();
  for (const auto &[eid, e] : game.entities().all()) CHECK_FALSE(isEnemy(e.kind));
  CHECK(health() == Vitals::kMax);

  registerEnemySpawns(game.spawner());
  game.advance(30.0f);
  for (const auto &[eid, e] : game.entities().all()) CHECK_FALSE(isEnemy(e.kind));
}

TEST_CASE_FIXTURE(EnemyGame, "at sunrise surface enemies under the sky vanish, grumbles stay") {
  // Both inside the strip that's open to the sky (x 4..7).
  EntityId gloom = spawnEnemy(EntityKind::Gloomling, {1.2f, 0.0f, -3.0f});
  EntityId crawler = spawnEnemy(EntityKind::ThornCrawler, {-1.2f, 0.0f, -5.0f});
  tick(); // still night
  CHECK(game.entities().find(gloom));
  CHECK(game.entities().find(crawler));

  run(SetTimeOfDay{kNoonSeconds});
  tick();
  CHECK_FALSE(game.entities().find(gloom));
  CHECK_FALSE(game.entities().find(crawler));
  CHECK(foe());
  for (const auto &[eid, e] : game.entities().all())
    CHECK(e.kind != EntityKind::DroppedItem); // vanishing drops nothing
}

TEST_CASE_FIXTURE(EnemyGame, "a surface enemy under a roof survives the day") {
  EntityId gloom = spawnEnemy(EntityKind::Gloomling, {2.0f, 0.0f, 0.0f});
  glm::ivec3 above(game.entities().find(gloom)->body.position);
  // A low roof over a sealed nook: no sky light reaches its feet.
  fill(game.world(), {above.x - 1, above.y - 1, above.z - 1},
       {above.x + 1, above.y + 1, above.z + 1}, BlockType::STONE);
  fill(game.world(), above, above, BlockType::AIR);
  REQUIRE(game.world().getSkyLightAt(above) < kSurfaceEnemyMinSkyLight);
  game.entities().find(gloom)->brain.reset();
  run(SetTimeOfDay{kNoonSeconds});
  tick();
  CHECK(game.entities().find(gloom));
}

TEST_CASE_FIXTURE(EnemyGame, "dying to an enemy respawns you with your items") {
  auto before = inventory().slots();
  bool died = false;
  for (int i = 0; i < 20 * 15 && !died; ++i) {
    tick();
    died = game.findPlayer(id)->vitals.dead();
  }
  REQUIRE(died);
  bool respawned = false;
  for (int i = 0; i < 20 * 4 && !respawned; ++i) {
    game.advance(Game::kTickSeconds);
    for (const Event &e : pollAll(conn)) respawned |= std::holds_alternative<Respawned>(e);
  }
  CHECK(respawned);
  CHECK(health() == Vitals::kMax);
  CHECK(inventory().slots() == before);
}

TEST_CASE_FIXTURE(EnemyGame, "the dead aren't attacked") {
  conn.send(::at(me_pos + glm::vec3(0, 40, 0), true));
  conn.send(::at(me_pos + glm::vec3(0, 39, 0), false));
  conn.send(::at(me_pos, true));
  game.pump();
  REQUIRE(game.findPlayer(id)->vitals.dead());
  pollAll(conn);
  if (Entity *e = foe()) e->body.position = me_pos + glm::vec3(0.0f, 0.0f, -0.8f);
  game.advance(Game::kTickSeconds);
  CHECK(knocks(pollAll(conn)).empty());
}

// ─── Multiplayer: players ──────────────────────────────────────────────

namespace {

// A host plus guests, each on its own in-process link.
struct PartyGame : SmallGame {
  LocalConnection host;
  std::vector<std::unique_ptr<LocalConnection>> guests;

  PartyGame() {
    game.addClient(host, Game::Role::Host);
    host.send(Join{"Host"});
    game.pump();
    pollAll(host);
  }

  // Joins a guest and returns everything it was sent.
  std::vector<Event> joinGuest(std::string name) {
    guests.push_back(std::make_unique<LocalConnection>());
    game.addClient(*guests.back(), Game::Role::Guest);
    guests.back()->send(Join{std::move(name)});
    game.pump();
    return pollAll(*guests.back());
  }
  LocalConnection &guest(size_t i = 0) { return *guests[i]; }

  std::vector<Event> send(LocalConnection &c, Command cmd) {
    c.send(std::move(cmd));
    game.pump();
    return pollAll(c);
  }

  template <class T>
  static const T *find(const std::vector<Event> &evs) {
    for (const Event &e : evs)
      if (auto *t = std::get_if<T>(&e)) return t;
    return nullptr;
  }
};

} // namespace

TEST_CASE("player names: letters, digits, _ and -, up to 16") {
  CHECK(Game::validName("Ada"));
  CHECK(Game::validName("kid_2-b"));
  CHECK(Game::validName("abcdefghijklmnop"));
  CHECK_FALSE(Game::validName(""));
  CHECK_FALSE(Game::validName("abcdefghijklmnopq"));
  CHECK_FALSE(Game::validName("has space"));
  CHECK_FALSE(Game::validName("emoji\xF0\x9F\x98\x80"));
}

TEST_CASE_FIXTURE(PartyGame, "a guest joins by name and the others are told") {
  auto evs = joinGuest("Bea");
  auto *joined = find<Joined>(evs);
  REQUIRE(joined);
  CHECK(game.findPlayer(joined->id)->name == "Bea");
  auto host_evs = pollAll(host);
  auto *told = find<PlayerJoined>(host_evs);
  REQUIRE(told);
  CHECK(told->id == joined->id);
  CHECK(told->name == "Bea");
  CHECK(game.playerCount() == 2);
}

TEST_CASE_FIXTURE(PartyGame, "an empty name becomes Player<id>") {
  auto evs = joinGuest("");
  auto *joined = find<Joined>(evs);
  REQUIRE(joined);
  CHECK(game.findPlayer(joined->id)->name == "Player" + std::to_string(joined->id));
}

TEST_CASE_FIXTURE(PartyGame, "a name already in the game is refused") {
  auto evs = joinGuest("Host");
  auto *refused = find<JoinRefused>(evs);
  REQUIRE(refused);
  CHECK(refused->reason == "Name already in use");
  CHECK_FALSE(find<Joined>(evs));
  CHECK(game.playerCount() == 1);
}

TEST_CASE_FIXTURE(PartyGame, "a bad name is refused") {
  auto evs = joinGuest("no spaces please");
  CHECK(find<JoinRefused>(evs));
  CHECK(game.playerCount() == 1);
}

TEST_CASE_FIXTURE(PartyGame, "the game holds at most 8 players") {
  for (int i = 1; i < static_cast<int>(Game::kMaxPlayers); ++i)
    REQUIRE(find<Joined>(joinGuest("g" + std::to_string(i))));
  auto evs = joinGuest("late");
  auto *refused = find<JoinRefused>(evs);
  REQUIRE(refused);
  CHECK(refused->reason == "Game is full");
  CHECK(game.playerCount() == Game::kMaxPlayers);
}

TEST_CASE_FIXTURE(PartyGame, "a leaving guest is removed and everyone is told") {
  PlayerId id = find<Joined>(joinGuest("Bea"))->id;
  pollAll(host);
  game.removeClient(guest());
  CHECK(game.playerCount() == 1);
  CHECK_FALSE(game.findPlayer(id));
  auto host_evs = pollAll(host);
  auto *left = find<PlayerLeft>(host_evs);
  REQUIRE(left);
  CHECK(left->id == id);
  // The name is free again.
  CHECK(find<Joined>(joinGuest("Bea")));
}

TEST_CASE_FIXTURE(PartyGame, "removing a link that never joined is harmless") {
  LocalConnection stray;
  game.addClient(stray, Game::Role::Guest);
  game.removeClient(stray);
  game.removeClient(stray);
  CHECK(pollAll(host).empty());
}

TEST_CASE_FIXTURE(PartyGame, "cheats are for the host only") {
  joinGuest("Bea");
  auto rejectedWith = [](const std::vector<Event> &evs) {
    auto *r = find<CommandRejected>(evs);
    return r ? r->reason : std::string();
  };
  CHECK(rejectedWith(send(guest(), GiveItem{ItemId::DIAMOND, 1})) == "give: host only");
  CHECK(rejectedWith(send(guest(), SetTimeOfDay{0.0f})) == "time: host only");
  CHECK(rejectedWith(send(guest(), SpawnDebugEntity{EntityKind::TestCreature})) ==
        "spawn: host only");
  CHECK(rejectedWith(send(guest(), Save{})) == "save: host only");
  CHECK(game.entities().size() == 0);

  CHECK(rejectedWith(send(host, GiveItem{ItemId::DIAMOND, 1})).empty());
}

TEST_CASE_FIXTURE(VitalsGame, "moves from before a respawn are ignored") {
  fall(40.0f);
  REQUIRE(me().vitals.dead());
  advance(Game::kRespawnDelayMs / 1000.0f + 0.1f);
  REQUIRE_FALSE(me().vitals.dead());
  REQUIRE(me().respawn == 1);
  glm::vec3 spawn = me().position;

  MovePlayer stale = at({50.5f, 100.0f, 50.5f}, true); // respawn 0: in flight
  send(stale);
  CHECK(me().position == spawn);

  MovePlayer fresh = stale;
  fresh.respawn = 1;
  send(fresh);
  CHECK(me().position == fresh.position);
}

// ─── Multiplayer: the shared world ─────────────────────────────────────

TEST_CASE_FIXTURE(PartyGame, "a guest is welcomed with the world before it joins") {
  game.world().loadChunkSync(0, 0);
  game.world().setBlockAt({3, 200, 3}, BlockType::GLOWSTONE);
  auto evs = joinGuest("Bea");
  REQUIRE(evs.size() >= 2);
  REQUIRE(std::holds_alternative<Welcome>(evs[0]));
  CHECK(std::holds_alternative<Joined>(evs[1]));
  const Welcome &w = std::get<Welcome>(evs[0]);
  CHECK(w.seed == g_settings.world_seed);
  CHECK(w.noise_offset == g_settings.noise_offset);
  CHECK(w.worldgen == g_settings.worldgen);
  CHECK(w.time_of_day == game.clock().time_of_day);
  CHECK(w.time_scale == g_settings.time_scale);
  CHECK(std::find(w.edits.begin(), w.edits.end(),
                  BlockEdit{{3, 200, 3}, BlockType::GLOWSTONE}) != w.edits.end());
}

TEST_CASE_FIXTURE(SmallGame, "the host isn't sent a Welcome") {
  LocalConnection host;
  game.addClient(host, Game::Role::Host);
  host.send(Join{"Host"});
  game.pump();
  for (const Event &e : pollAll(host)) CHECK_FALSE(std::holds_alternative<Welcome>(e));
}

TEST_CASE_FIXTURE(PartyGame, "guests get the time once a second; the host doesn't") {
  joinGuest("Bea");
  pollAll(host);
  game.advance(Game::kTickSeconds * (Game::kTimeSyncTicks * 3));
  int syncs = 0;
  float last = -1.0f;
  for (const Event &e : pollAll(guest()))
    if (auto *t = std::get_if<TimeSync>(&e)) {
      ++syncs;
      last = t->time_of_day;
      CHECK(t->time_scale == g_settings.time_scale);
    }
  CHECK(syncs == 3);
  CHECK(last == game.clock().time_of_day);
  for (const Event &e : pollAll(host)) CHECK_FALSE(std::holds_alternative<TimeSync>(e));
}

TEST_CASE_FIXTURE(PartyGame, "snapshots show the other players but not yourself") {
  PlayerId bea = PartyGame::find<Joined>(joinGuest("Bea"))->id;
  PlayerId host_id = 0;
  for (PlayerId id = 1; id < 10; ++id)
    if (game.findPlayer(id) && game.findPlayer(id)->name == "Host") host_id = id;
  glm::vec3 near = game.findPlayer(host_id)->position + glm::vec3(2, 0, 0);
  MovePlayer move{near, 30.0f, -10.0f, false, true};
  send(guest(), move);
  pollAll(host);

  game.advance(Game::kTickSeconds);
  auto last = [](const std::vector<Event> &evs) {
    const EntitySnapshot *snap = nullptr;
    for (const Event &e : evs)
      if (auto *s = std::get_if<EntitySnapshot>(&e)) snap = s;
    REQUIRE(snap);
    return *snap;
  };
  EntitySnapshot to_host = last(pollAll(host));
  REQUIRE(to_host.players.size() == 1);
  const PlayerView &v = to_host.players[0];
  CHECK(v.id == bea);
  CHECK(v.name == "Bea");
  CHECK(v.position == near);
  CHECK(v.yaw == 30.0f);
  CHECK(v.pitch == -10.0f);
  CHECK(v.crouch);

  EntitySnapshot to_bea = last(pollAll(guest()));
  REQUIRE(to_bea.players.size() == 1);
  CHECK(to_bea.players[0].id == host_id);
}

// ─── Multiplayer: guest records ────────────────────────────────────────

namespace {

// A party bound to a save directory, so guests' records hit the disk.
struct SavedParty : PartyGame {
  ScopedSeed seed;
  TempDir dir;
  SavedParty() { game.load(dir.path.string()); } // nothing there yet: binds the dir

  PlayerId joinedId(const std::vector<Event> &evs) {
    auto *j = find<Joined>(evs);
    REQUIRE(j);
    return j->id;
  }
};

} // namespace

TEST_CASE_FIXTURE(SavedParty, "a guest who leaves is saved and comes back with their stuff") {
  game.setMode(GameMode::Creative); // so the saved flying is kept
  PlayerId id = joinedId(joinGuest("Bea"));
  MovePlayer move{{5.5f, 90.0f, 6.5f}, 12.0f, -4.0f, true};
  send(guest(), move);
  send(guest(), SelectSlot{4});
  const Inventory before = game.findPlayer(id)->inventory;

  game.removeClient(guest());
  CHECK(std::filesystem::exists(dir.path / "players" / "Bea.dat"));

  auto evs = joinGuest("Bea");
  auto *j = find<Joined>(evs);
  REQUIRE(j);
  CHECK(j->position == move.position);
  CHECK(j->yaw == 12.0f);
  CHECK(j->pitch == -4.0f);
  CHECK(j->slot == 4);
  CHECK(j->fly);
  CHECK(game.findPlayer(j->id)->inventory.slots() == before.slots());
}

TEST_CASE_FIXTURE(SavedParty, "a new name starts at spawn with the starter kit") {
  PlayerId id = joinedId(joinGuest("New"));
  CHECK(game.findPlayer(id)->inventory.slots() == Game::starterKit().slots());
  CHECK_FALSE(game.guestRecord("New"));
}

TEST_CASE_FIXTURE(SavedParty, "the host's save writes guests still playing, and load reads them") {
  joinGuest("Bea");
  MovePlayer move{{1.5f, 95.0f, 2.5f}, 0.0f, 0.0f};
  send(guest(), move);
  REQUIRE(game.save());
  CHECK(std::filesystem::exists(dir.path / "players" / "Bea.dat"));

  Game reloaded;
  reloaded.load(dir.path.string());
  const auto *rec = reloaded.guestRecord("Bea");
  REQUIRE(rec);
  CHECK(rec->player.position == move.position);
}

TEST_CASE_FIXTURE(SavedParty, "the host never takes a guest's record") {
  joinGuest("Bea");
  game.removeClient(guest());
  REQUIRE(game.guestRecord("Bea"));
  LocalConnection host2;
  game.removeClient(host);
  game.addClient(host2, Game::Role::Host);
  host2.send(Join{"Bea"}); // same name as the record, but the host
  game.pump();
  auto evs = pollAll(host2);
  auto *j = find<Joined>(evs);
  REQUIRE(j);
  CHECK(game.findPlayer(j->id)->inventory.slots() == Game::starterKit().slots());
}

// ─── Creative mode ─────────────────────────────────────────────────────

TEST_CASE_FIXTURE(EditGame, "creative: placing uses nothing up and breaking drops nothing") {
  game.setMode(GameMode::Creative);
  run(GiveItem{itemForBlock(BlockType::COBBLESTONE), 1});
  int slot = -1;
  for (int i = 0; i < kHotbarSlots; ++i)
    if (inventory().slot(i).id == itemForBlock(BlockType::COBBLESTONE)) slot = i;
  REQUIRE(slot >= 0);
  run(SelectSlot{slot});
  for (int x = 5; x < 8; ++x) CHECK(changes(run(PlaceBlock{{x, ground + 1, 5}})) == 1);
  CHECK(inventory().slot(slot) == ItemStack{itemForBlock(BlockType::COBBLESTONE), 1});

  size_t entities_before = game.entities().size();
  Inventory before = inventory();
  CHECK(changes(mine({5, ground + 1, 5})) == 1);
  CHECK(at({5, ground + 1, 5}) == BlockType::AIR);
  CHECK(inventory().slots() == before.slots());
  CHECK(game.entities().size() == entities_before);
}

TEST_CASE_FIXTURE(EditGame, "creative: every building block can be placed") {
  game.setMode(GameMode::Creative);
  const glm::ivec3 spot{5, ground + 1, 5};
  for (auto v = static_cast<uint16_t>(BlockType::OAK_PLANKS);
       v <= static_cast<uint16_t>(BlockType::PINK_BLOCK); ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(v);
    CHECK(changes(place(spot, b)) == 1);
    CHECK(at(spot) == b);
    mine(spot);
    run(DropItem{true}); // free the slot: there are more blocks than hotbar slots
  }
}

TEST_CASE_FIXTURE(EditGame, "survival: a placed brick breaks into one brick") {
  game.setMode(GameMode::Creative);
  REQUIRE(changes(place({5, ground + 1, 5}, BlockType::BRICKS)) == 1);

  game.setMode(GameMode::Survival);
  run(SelectSlot{9});
  wield(ItemId::STONE_PICKAXE);
  int before = countOf(inventory(), itemForBlock(BlockType::BRICKS));
  CHECK(changes(mine({5, ground + 1, 5})) == 1);
  CHECK(countOf(inventory(), itemForBlock(BlockType::BRICKS)) == before + 1);
}

// ─── Falling out of the world ──────────────────────────────────────────

TEST_CASE_FIXTURE(EditGame, "a player below the world is put back on top of the same column") {
  MovePlayer fell;
  fell.position = {5.5f, -1746.0f, 5.5f};
  auto evs = run(fell);
  const Respawned *back = nullptr;
  for (const Event &e : evs)
    if (const auto *r = std::get_if<Respawned>(&e)) back = r;
  REQUIRE(back);
  int top = kChunkHeight - 1;
  while (at({5, top, 5}) == BlockType::AIR) --top;
  CHECK(back->position == glm::vec3(5.5f, top + 1.0f, 5.5f));
  CHECK(game.findPlayer(id)->position == back->position);

  // Moves still in flight from down there are stale; nobody got hurt.
  run(fell);
  CHECK(game.findPlayer(id)->position == back->position);
  CHECK(game.findPlayer(id)->vitals.health() == Vitals::kMax);
}

TEST_CASE_FIXTURE(VitalsGame, "creative: no fall damage, drowning or stamina drain") {
  game.setMode(GameMode::Creative);
  fall(10.0f);
  CHECK(me().vitals.health() == Vitals::kMax);

  MovePlayer under = at({0.5f, 60.0f, 0.5f}, false);
  under.in_water = under.eyes_underwater = true;
  under.sprinting = true;
  send(under);
  advance(15.0f);
  CHECK(me().vitals.breath() == Vitals::kMax);
  CHECK(me().vitals.stamina() == Vitals::kMax);
  CHECK(me().vitals.health() == Vitals::kMax);
}

TEST_CASE_FIXTURE(EnemyGame, "creative: enemies don't hit or chase you") {
  game.setMode(GameMode::Creative);
  for (int i = 0; i < 40; ++i) CHECK(knocks(tick()).empty());
  CHECK(health() == Vitals::kMax);

  // Let it roam: with no targets it never closes in to touch the player.
  game.advance(5.0f);
  CHECK(health() == Vitals::kMax);
}

TEST_CASE_FIXTURE(PartyGame, "creative: guests may use the palette, other cheats stay the host's") {
  game.setMode(GameMode::Creative);
  joinGuest("Bea");
  CHECK_FALSE(find<CommandRejected>(send(guest(), GiveItem{ItemId::DIAMOND, 64})));
  CHECK(find<CommandRejected>(send(guest(), SetTimeOfDay{0.0f})));
  CHECK(find<CommandRejected>(send(guest(), Save{})));
}

TEST_CASE_FIXTURE(PartyGame, "Joined tells every player the world's mode") {
  CHECK(find<Joined>(joinGuest("Bea"))->mode == GameMode::Survival);
  game.setMode(GameMode::Creative);
  CHECK(find<Joined>(joinGuest("Cy"))->mode == GameMode::Creative);
}

TEST_CASE_FIXTURE(SmallGame, "a saved flyer lands in a survival world") {
  WorldSave::PlayerData pd;
  pd.position = {0.5f, 120.0f, 0.5f};
  pd.fly_mode = 1;
  game.restore({}, {}, pd);
  LocalConnection conn;
  CHECK_FALSE(join(game, conn).fly);

  SmallGame creative;
  WorldSave::LevelData level;
  level.mode = GameMode::Creative;
  creative.game.restore(level, {}, pd);
  LocalConnection c2;
  CHECK(join(creative.game, c2).fly);
}

TEST_CASE_FIXTURE(PartyGame, "a guest crafts at the host's table, checked the same way") {
  game.world().loadChunkSync(0, 0);
  PlayerId bea_id = find<Joined>(joinGuest("Bea"))->id;
  PlayerState &bea = *game.findPlayer(bea_id);
  bea.inventory.set(0, {itemForBlock(BlockType::COBBLESTONE), 4});
  see(bea.seen, itemForBlock(BlockType::COBBLESTONE));
  CHECK(reason(send(guest(), Craft{kStoneBricksRecipe, false})) ==
        "craft: needs a Crafting Table nearby");
  glm::ivec3 near = glm::ivec3(glm::floor(bea.position)) + glm::ivec3(1, 0, 0);
  game.world().setBlockAt(near, BlockType::CRAFTING_TABLE);
  send(guest(), Craft{kStoneBricksRecipe, false});
  CHECK(heldCount(bea.inventory, itemForBlock(BlockType::STONE_BRICKS)) == 4);
}

TEST_CASE_FIXTURE(PartyGame, "two players looking in the same furnace both see it change") {
  game.world().loadChunkSync(0, 0);
  PlayerId bea_id = find<Joined>(joinGuest("Bea"))->id;
  PlayerState &bea = *game.findPlayer(bea_id);
  glm::ivec3 at_ = glm::ivec3(glm::floor(bea.position)) + glm::ivec3(1, 0, 0);
  game.world().setBlockAt(at_, BlockType::FURNACE);
  PlayerId host_id = 0;
  for (PlayerId pid = 1; pid < 10; ++pid)
    if (game.findPlayer(pid) && pid != bea_id) host_id = pid;
  REQUIRE(host_id);
  game.findPlayer(host_id)->position = bea.position; // stand together
  send(host, OpenContainer{at_});
  send(guest(), OpenContainer{at_});
  bea.inventory.set(0, {ItemId::RAW_COPPER, 2});
  send(guest(), MoveItems{0, kFurnaceInput, 0});
  game.pump();
  std::vector<Event> seen_by_host;
  host.poll(seen_by_host);
  const ContainerView *v = nullptr;
  for (const Event &e : seen_by_host)
    if (const auto *c = std::get_if<ContainerView>(&e)) v = c;
  REQUIRE(v);
  CHECK(v->input == ItemStack{ItemId::RAW_COPPER, 2});
}

// ─── Fishing ───────────────────────────────────────────────────────────

namespace {

// EditGame with a pond 2 deep just in front (-Z) of the player, who stands
// on stone with a rod in hand.
struct FishingGame : EditGame {
  FishingGame() {
    auto put = [&](int x, int y, int z, BlockType b) {
      if (at({x, y, z}) != b) game.world().setBlockAt({x, y, z}, b);
    };
    for (int x = 1; x <= 10; ++x)
      for (int z = 0; z <= 11; ++z) {
        bool pond = z <= 6;
        put(x, ground - 1, z, pond ? BlockType::WATER : BlockType::STONE);
        put(x, ground, z, pond ? BlockType::WATER : BlockType::STONE);
        for (int y = ground + 1; y <= ground + 5; ++y) put(x, y, z, BlockType::AIR);
      }
    moveTo({5.5f, ground + 1.0f, 8.5f});
    wield(ItemId::FISHING_ROD);
  }

  const PlayerState &me() { return *game.findPlayer(id); }
  Entity *bobber() {
    return me().fishing ? game.entities().find(me().fishing->bobber) : nullptr;
  }
  std::vector<Event> cast() { return run(CastLine{{0.0f, 0.2f, -1.0f}}); }
  // Ticks until the bobber bites (or the time runs out); false if it never did.
  bool waitForBite(float seconds) {
    for (int t = 0; t < static_cast<int>(seconds * 20); ++t) {
      game.tick();
      if (bobber() && bobber()->biting) return true;
    }
    return false;
  }
  static const Caught *caught(const std::vector<Event> &evs) {
    for (const Event &e : evs)
      if (const auto *c = std::get_if<Caught>(&e)) return c;
    return nullptr;
  }
};

} // namespace

TEST_CASE_FIXTURE(FishingGame, "a cast lands in the pond and floats on top") {
  CHECK(cast().empty());
  REQUIRE(bobber());
  CHECK(bobber()->owner == id);
  for (int t = 0; t < 40; ++t) game.tick();
  REQUIRE(bobber());
  glm::vec3 p = bobber()->body.position;
  CHECK(p.z < 7.0f);   // out over the water
  CHECK(p.z > 0.0f);
  CHECK(p.y == doctest::Approx(ground + 1.0f - 0.12f)); // at the surface
  CHECK(me().fishing->bite_at_ms > 0);
}

TEST_CASE_FIXTURE(FishingGame, "one bobber each: a second cast replaces the first") {
  cast();
  EntityId first = me().fishing->bobber;
  cast();
  CHECK(me().fishing->bobber != first);
  CHECK_FALSE(game.entities().find(first));
  int bobbers = 0;
  for (const auto &[eid, e] : game.entities().all()) bobbers += e.kind == EntityKind::Bobber;
  CHECK(bobbers == 1);
}

TEST_CASE_FIXTURE(FishingGame, "casting needs a rod and a real direction") {
  wield(ItemId::STICKS);
  CHECK(rejected(cast()));
  wield(ItemId::FISHING_ROD);
  CHECK(rejected(run(CastLine{{0.0f, 0.0f, 0.0f}})));
  CHECK(rejected(run(CastLine{{std::nanf(""), 0.0f, -1.0f}})));
  CHECK(rejected(run(ReelIn{}))); // no line out
  CHECK_FALSE(me().fishing);
}

TEST_CASE_FIXTURE(FishingGame, "reeling in during a bite catches a fish and wears the rod") {
  cast();
  REQUIRE(waitForBite(kBiteMaxMs / 1000.0f + 2.0f));
  pollAll(conn);
  auto evs = run(ReelIn{});
  const Caught *c = caught(evs);
  REQUIRE(c);
  CHECK(heldCount(inventory(), c->item) >= 1);
  CHECK(inventory().slot(me().selected_slot) == ItemStack{ItemId::FISHING_ROD, 1, 1});
  CHECK_FALSE(me().fishing);
  CHECK_FALSE(bobber());
}

TEST_CASE_FIXTURE(FishingGame, "reeling in early, or from land, brings up nothing and costs nothing") {
  cast();
  for (int t = 0; t < 40; ++t) game.tick(); // landed, but no bite before 5 s
  pollAll(conn);
  auto evs = run(ReelIn{});
  CHECK_FALSE(caught(evs));
  CHECK_FALSE(me().fishing);
  CHECK(inventory().slot(me().selected_slot) == ItemStack{ItemId::FISHING_ROD, 1, 0});

  // On land: straight down at the player's feet, then wait long enough.
  run(CastLine{{0.0f, -1.0f, 0.0f}});
  CHECK_FALSE(waitForBite(kBiteMaxMs / 1000.0f + 2.0f));
  CHECK_FALSE(caught(run(ReelIn{})));
}

TEST_CASE_FIXTURE(FishingGame, "a missed bite swims off and another comes later") {
  cast();
  REQUIRE(waitForBite(kBiteMaxMs / 1000.0f + 2.0f));
  // Let the window pass.
  for (int t = 0; t < (kBiteWindowMs + 400) / Game::kTickMs; ++t) game.tick();
  REQUIRE(bobber());
  CHECK_FALSE(bobber()->biting);
  CHECK(me().fishing->bite_at_ms > 0);
  CHECK(waitForBite(kBiteMaxMs / 1000.0f + 2.0f)); // still on the line
}

TEST_CASE_FIXTURE(FishingGame, "a fish near the bobber halves the wait") {
  for (int i = 0; i < 6; ++i) {
    cast();
    for (int t = 0; t < 40; ++t) game.tick();
    REQUIRE(bobber());
    auto fish = game.entities().spawn(EntityKind::Pebblefish,
                                      bobber()->body.position - glm::vec3(0.0f, 0.8f, 0.0f));
    REQUIRE(fish);
    // Re-roll with the fish there, the way a new landing would.
    game.findPlayer(id)->fishing->bite_at_ms = 0;
    CHECK(waitForBite(kBiteMaxMs / 2000.0f + 0.5f));
    game.entities().remove(*fish);
    run(ReelIn{});
  }
}

TEST_CASE_FIXTURE(FishingGame, "the line breaks on switching item, walking away or dying") {
  cast();
  REQUIRE(bobber());
  wield(ItemId::STICKS);
  game.tick();
  CHECK_FALSE(me().fishing);
  CHECK_FALSE(bobber());

  wield(ItemId::FISHING_ROD);
  cast();
  for (int t = 0; t < 40; ++t) game.tick();
  REQUIRE(bobber());
  moveTo({5.5f, ground + 1.0f, 8.5f + kLineBreakDistance + 2.0f});
  game.tick();
  CHECK_FALSE(me().fishing);

  moveTo({5.5f, ground + 1.0f, 8.5f});
  cast();
  game.findPlayer(id)->vitals.damage(Vitals::kMax);
  game.tick();
  CHECK_FALSE(me().fishing);
  for (const auto &[eid, e] : game.entities().all()) CHECK(e.kind != EntityKind::Bobber);
}

TEST_CASE_FIXTURE(FishingGame, "leaving takes your bobber with you") {
  cast();
  REQUIRE(bobber());
  game.removeClient(conn);
  for (const auto &[eid, e] : game.entities().all()) CHECK(e.kind != EntityKind::Bobber);
}

TEST_CASE_FIXTURE(FishingGame, "a full inventory drops the catch at your feet") {
  PlayerState &p = *game.findPlayer(id);
  for (int i = 0; i < kInventorySlots; ++i)
    if (i != p.selected_slot) p.inventory.set(i, {itemForBlock(BlockType::DIRT), 64});
  cast();
  REQUIRE(waitForBite(kBiteMaxMs / 1000.0f + 2.0f));
  pollAll(conn);
  auto evs = run(ReelIn{});
  const Caught *c = caught(evs);
  REQUIRE(c);
  bool dropped = false;
  for (const auto &[eid, e] : game.entities().all())
    dropped |= e.kind == EntityKind::DroppedItem && e.item.id == c->item;
  CHECK(dropped);
}

TEST_CASE_FIXTURE(FishingGame, "the rod breaks after its last catch, and creative never wears it") {
  game.findPlayer(id)->inventory.set(game.findPlayer(id)->selected_slot,
                                     {ItemId::FISHING_ROD, 1, 63});
  cast();
  REQUIRE(waitForBite(kBiteMaxMs / 1000.0f + 2.0f));
  pollAll(conn);
  auto evs = run(ReelIn{});
  CHECK(caught(evs));
  CHECK(inventory().slot(me().selected_slot).empty());
  bool broke = false;
  for (const Event &e : evs)
    if (const auto *b = std::get_if<ToolBroke>(&e)) broke = b->item == ItemId::FISHING_ROD;
  CHECK(broke);

  game.setMode(GameMode::Creative);
  wield(ItemId::FISHING_ROD);
  cast();
  REQUIRE(waitForBite(kBiteMaxMs / 1000.0f + 2.0f));
  run(ReelIn{});
  CHECK(inventory().slot(me().selected_slot) == ItemStack{ItemId::FISHING_ROD, 1, 0});
}

TEST_CASE_FIXTURE(FishingGame, "fish can't be hit") {
  auto fish = game.entities().spawn(EntityKind::Sunfin, glm::vec3(5.5f, ground + 0.2f, 7.0f));
  REQUIRE(fish);
  CHECK(rejected(run(AttackEntity{*fish})));
  CHECK(game.entities().find(*fish));
}

TEST_CASE_FIXTURE(FishingGame, "bobbers in snapshots carry their owner and bite") {
  cast();
  REQUIRE(waitForBite(kBiteMaxMs / 1000.0f + 2.0f));
  pollAll(conn);
  game.tick();
  bool seen = false;
  for (const Event &e : pollAll(conn))
    if (const auto *s = std::get_if<EntitySnapshot>(&e))
      for (const EntityView &v : s->entities)
        if (v.kind == EntityKind::Bobber) seen = v.owner == id && v.biting;
  CHECK(seen);
}

// ─── Chests ────────────────────────────────────────────────────────────

TEST_CASE_FIXTURE(EditGame, "a chest takes anything, and shows every viewer") {
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::CHEST);
  REQUIRE(game.chestAt(at_));
  PlayerState &me = *game.findPlayer(id);
  me.inventory.set(20, {itemForBlock(BlockType::DIRT), 30});
  me.inventory.set(21, {ItemId::WOOD_SWORD, 1, 5});
  CHECK(reason(run(MoveItems{20, kContainerSlot0, 0})) == "move: nothing open");

  std::optional<ContainerView> v = lastView(run(OpenContainer{at_}));
  REQUIRE(v);
  CHECK(v->kind == ContainerKind::Chest);
  CHECK(v->slots.size() == static_cast<size_t>(kChestSlots));

  run(MoveItems{20, kContainerSlot0 + 19, 10});
  auto evs = run(MoveItems{21, kContainerSlot0 + 3, 0});
  Chest &c = *game.chestAt(at_);
  CHECK(c[19] == ItemStack{itemForBlock(BlockType::DIRT), 10});
  CHECK(c[3] == ItemStack{ItemId::WOOD_SWORD, 1, 5});
  CHECK(me.inventory.slot(20) == ItemStack{itemForBlock(BlockType::DIRT), 20});
  REQUIRE(lastView(evs));
  CHECK(lastView(evs)->slots[3] == c[3]);
  // Out again, and nothing past the chest's last slot.
  run(MoveItems{kContainerSlot0 + 3, 30, 0});
  CHECK(me.inventory.slot(30) == ItemStack{ItemId::WOOD_SWORD, 1, 5});
  CHECK(reason(run(MoveItems{20, kContainerSlot0 + kChestSlots, 0})) == "move: not allowed");
  // Furnace slot rules don't apply: dirt goes in slot 2 (a furnace's output).
  CHECK(run(MoveItems{20, kContainerSlot0 + 2, 1}).size() >= 1);
  CHECK(c[2] == ItemStack{itemForBlock(BlockType::DIRT), 1});
}

TEST_CASE_FIXTURE(EditGame, "breaking a chest spills it; walking away closes it") {
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::CHEST);
  (*game.chestAt(at_))[0] = {ItemId::DIAMOND, 3};
  run(OpenContainer{at_});
  moveTo({5.5f, ground + 1.0f, 15.5f});
  game.tick();
  std::optional<ContainerView> v = lastView(pollAll(conn));
  REQUIRE(v);
  CHECK_FALSE(v->open);

  moveTo({5.5f, ground + 1.0f, 8.5f});
  wield(ItemId::WOOD_AXE);
  mine(at_);
  CHECK_FALSE(game.chestAt(at_));
  CHECK(heldCount(inventory(), itemForBlock(BlockType::CHEST)) == 1);
  bool spilled = false;
  for (const auto &[eid, e] : game.entities().all())
    spilled |= e.kind == EntityKind::DroppedItem && e.item == ItemStack{ItemId::DIAMOND, 3};
  CHECK(spilled);
}

TEST_CASE_FIXTURE(EditGame, "chests are saved with the world") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string()));
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::CHEST);
  (*game.chestAt(at_))[7] = {ItemId::RUBY, 2};
  run(Save{});

  ScopedRenderDistance rd2{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  REQUIRE(loaded.chestAt(at_));
  CHECK((*loaded.chestAt(at_))[7] == ItemStack{ItemId::RUBY, 2});
}

TEST_CASE_FIXTURE(EditGame, "a treasure chest needs a key, then fills with loot") {
  glm::ivec3 at_{5, ground + 1, 5};
  game.world().setBlockAt(at_, BlockType::TREASURE_CHEST);
  PlayerState &me = *game.findPlayer(id);

  // No key: refused, and nothing changes.
  CHECK(reason(run(OpenContainer{at_})) == "open: locked (needs a Treasure Key)");
  CHECK(at(at_) == BlockType::TREASURE_CHEST);
  CHECK_FALSE(game.chestAt(at_));

  // Can't be broken open, in survival or creative.
  wield(ItemId::DIAMOND_PICKAXE);
  CHECK(reason(run(BreakBlock{at_})) == "break: unbreakable block");
  game.setMode(GameMode::Creative);
  CHECK(reason(run(BreakBlock{at_})) == "break: unbreakable block");
  game.setMode(GameMode::Survival);
  CHECK(at(at_) == BlockType::TREASURE_CHEST);

  me.inventory.set(me.selected_slot, {ItemId::TREASURE_KEY, 2});
  auto evs = run(OpenContainer{at_});
  CHECK(me.inventory.slot(me.selected_slot) == ItemStack{ItemId::TREASURE_KEY, 1});
  CHECK(at(at_) == BlockType::CHEST);
  REQUIRE(game.chestAt(at_));
  int stacks = 0;
  for (const ItemStack &s : *game.chestAt(at_)) stacks += !s.empty();
  CHECK(stacks >= kTreasureMinStacks);
  CHECK(stacks <= kTreasureMaxStacks);
  std::optional<ContainerView> v = lastView(evs);
  REQUIRE(v);
  CHECK(v->open);
  CHECK(v->kind == ContainerKind::Chest);
  CHECK(me.open_container == at_);
  // Everyone hears the block change.
  bool told = false;
  for (const Event &e : pollAll(other))
    if (const auto *b = std::get_if<BlockChanged>(&e))
      told |= b->pos == at_ && b->type == BlockType::CHEST;
  CHECK(told);

  // From now on it's an ordinary chest: opening again costs no key.
  run(CloseContainer{});
  run(OpenContainer{at_});
  CHECK(me.inventory.slot(me.selected_slot) == ItemStack{ItemId::TREASURE_KEY, 1});
}

TEST_CASE_FIXTURE(EditGame, "two players see the same chest") {
  glm::ivec3 at_{5, ground + 1, 5};
  place(at_, BlockType::CHEST);
  // The observer walks over and looks in too.
  other.send(MovePlayer{{6.5f, ground + 1.0f, 8.5f}, 0.0f, 0.0f, false, false});
  game.pump();
  other.send(OpenContainer{at_});
  game.pump();
  pollAll(other);
  run(OpenContainer{at_});
  game.findPlayer(id)->inventory.set(20, {ItemId::COAL, 9});
  run(MoveItems{20, kContainerSlot0, 0});
  std::optional<ContainerView> v = lastView(pollAll(other));
  REQUIRE(v);
  CHECK(v->slots[0] == ItemStack{ItemId::COAL, 9});
}

// ─── Doors ─────────────────────────────────────────────────────────────

namespace {
struct DoorGame : EditGame {
  glm::ivec3 door{5, ground + 1, 5};
  glm::ivec3 top() const { return door + glm::ivec3(0, 1, 0); }

  // Holds `n` doors and places one at `pos`, looking along `yaw`.
  std::vector<Event> placeDoor(glm::ivec3 pos, float yaw = 0.0f, uint16_t n = 1) {
    conn.send(MovePlayer{{5.5f, ground + 1.0f, 8.5f}, yaw, 0.0f, false, false});
    game.pump();
    PlayerState &me = *game.findPlayer(id);
    me.inventory.set(me.selected_slot, {ItemId::WOODEN_DOOR, n});
    pollAll(conn);
    return run(PlaceBlock{pos});
  }
};
} // namespace

TEST_CASE_FIXTURE(DoorGame, "a door is placed closed, two high, facing its placer, and used up") {
  auto evs = placeDoor(door, 0.0f, 2); // looking +X
  CHECK(changes(evs) == 2);
  CHECK(at(door) == doorBlock(false, DoorFacing::East, false));
  CHECK(at(top()) == doorBlock(true, DoorFacing::East, false));
  const PlayerState &me = *game.findPlayer(id);
  CHECK(me.inventory.slot(me.selected_slot) == ItemStack{ItemId::WOODEN_DOOR, 1});
  CHECK(changes(pollAll(other)) == 2);

  // Looking the other ways.
  glm::ivec3 next = door + glm::ivec3(-2, 0, 0);
  game.world().setBlockAt(next - glm::ivec3(0, 1, 0), BlockType::STONE);
  game.world().setBlockAt(next, BlockType::AIR);
  game.world().setBlockAt(next + glm::ivec3(0, 1, 0), BlockType::AIR);
  placeDoor(next, -90.0f);
  CHECK(at(next) == doorBlock(false, DoorFacing::North, false));
}

TEST_CASE_FIXTURE(DoorGame, "a door needs ground under it and two clear cells") {
  // No room: something in the top cell.
  game.world().setBlockAt(top(), BlockType::STONE);
  CHECK(reason(placeDoor(door)) == "place: no room for a door");
  CHECK(at(door) == BlockType::AIR);
  const PlayerState &me = *game.findPlayer(id);
  CHECK(me.inventory.slot(me.selected_slot) == ItemStack{ItemId::WOODEN_DOOR, 1});
  // Nothing under it.
  game.world().setBlockAt(top(), BlockType::AIR);
  CHECK(reason(placeDoor(top())) == "place: a door needs ground under it");
  // Not on another door either.
  placeDoor(door);
  CHECK(reason(placeDoor(top() + glm::ivec3(0, 1, 0))) == "place: a door needs ground under it");
}

TEST_CASE_FIXTURE(DoorGame, "either half opens and closes the whole door, for everyone") {
  placeDoor(door);
  pollAll(other);
  CHECK(changes(run(UseDoor{door})) == 2);
  CHECK(doorOpen(at(door)));
  CHECK(doorOpen(at(top())));
  CHECK(changes(pollAll(other)) == 2);
  run(UseDoor{top()});
  CHECK_FALSE(doorOpen(at(door)));
  CHECK_FALSE(doorOpen(at(top())));
  CHECK(doorFacing(at(door)) == DoorFacing::East); // it only swings

  CHECK(reason(run(UseDoor{door + glm::ivec3(1, 0, 0)})) == "door: that isn't a door");
  moveTo({5.5f, ground + 1.0f, 5.5f + 9.0f});
  CHECK(reason(run(UseDoor{door})) == "door: out of reach");
  CHECK_FALSE(doorOpen(at(door)));
}

TEST_CASE_FIXTURE(DoorGame, "a door won't shut on someone standing in it") {
  placeDoor(door);
  run(UseDoor{door});
  moveTo({5.5f, ground + 1.0f, 5.5f}); // in the doorway
  CHECK(reason(run(UseDoor{door})) == "door: someone's in the way");
  CHECK(doorOpen(at(door)));
}

TEST_CASE_FIXTURE(DoorGame, "breaking either half takes the whole door and drops one") {
  for (glm::ivec3 hit : {door, door + glm::ivec3(0, 1, 0)}) {
    CAPTURE(hit.y - door.y);
    placeDoor(door);
    PlayerState &me = *game.findPlayer(id);
    me.inventory.set(me.selected_slot, {});
    auto evs = mine(hit);
    CHECK(changes(evs) == 2);
    CHECK(at(door) == BlockType::AIR);
    CHECK(at(top()) == BlockType::AIR);
    CHECK(heldCount(inventory(), ItemId::WOODEN_DOOR) == 1);
    me.inventory.set(me.selected_slot, {});
    for (int i = 0; i < kInventorySlots; ++i) me.inventory.set(i, {});
  }
}

TEST_CASE_FIXTURE(DoorGame, "a door pops off when the ground under it goes") {
  placeDoor(door);
  wield(ItemId::DIAMOND_SHOVEL);
  glm::ivec3 ground_cell = door - glm::ivec3(0, 1, 0);
  // Whatever the ground is, a diamond pickaxe or shovel gets through it quickly.
  if (toughness(at(ground_cell)).best == ToolKind::Pickaxe) wield(ItemId::DIAMOND_PICKAXE);
  auto evs = mine(ground_cell);
  CHECK(changes(evs) == 3);
  CHECK(at(door) == BlockType::AIR);
  CHECK(at(top()) == BlockType::AIR);
  CHECK(dropped(game, ItemId::WOODEN_DOOR) == 1);
}

TEST_CASE_FIXTURE(DoorGame, "doors are saved open or shut") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string()));
  placeDoor(door);
  run(UseDoor{door});
  run(Save{});

  ScopedRenderDistance rd{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  loaded.world().loadChunkSync(0, 0);
  loaded.world().flushPendingEdits();
  CHECK(loaded.world().getBlockAt(glm::vec3(door) + 0.5f) ==
        doorBlock(false, DoorFacing::East, true));
  CHECK(loaded.world().getBlockAt(glm::vec3(top()) + 0.5f) ==
        doorBlock(true, DoorFacing::East, true));
}

// ─── Beds ──────────────────────────────────────────────────────────────

namespace {
struct BedGame : EditGame {
  glm::ivec3 foot{5, ground + 1, 5};
  glm::ivec3 head() const { return foot + glm::ivec3(1, 0, 0); } // looking +X

  BedGame() {
    // A flat floor round the bed, clear above it, so the terrain doesn't matter.
    for (int x = 2; x <= 9; ++x)
      for (int z = 2; z <= 9; ++z) {
        game.world().setBlockAt({x, ground, z}, BlockType::STONE);
        for (int y = ground + 1; y <= ground + 4; ++y)
          game.world().setBlockAt({x, y, z}, BlockType::AIR);
      }
  }

  // Holds `n` beds of `item` and places one at `pos`, looking along `yaw`.
  std::vector<Event> placeBed(glm::ivec3 pos, float yaw = 0.0f, uint16_t n = 1,
                              ItemId item = ItemId::RED_BED) {
    conn.send(MovePlayer{{5.5f, ground + 1.0f, 8.5f}, yaw, 0.0f, false, false});
    game.pump();
    PlayerState &me = *game.findPlayer(id);
    me.inventory.set(me.selected_slot, {item, n});
    pollAll(conn);
    return run(PlaceBlock{pos});
  }
};
} // namespace

TEST_CASE_FIXTURE(BedGame, "a bed is placed foot first, its head the way you look, and used up") {
  auto evs = placeBed(foot, 0.0f, 2); // looking +X
  CHECK(changes(evs) == 2);
  CHECK(at(foot) == bedBlock(BedColour::Red, false, DoorFacing::East));
  CHECK(at(head()) == bedBlock(BedColour::Red, true, DoorFacing::East));
  const PlayerState &me = *game.findPlayer(id);
  CHECK(me.inventory.slot(me.selected_slot) == ItemStack{ItemId::RED_BED, 1});
  CHECK(changes(pollAll(other)) == 2);

  // Looking north (-Z), in another colour.
  glm::ivec3 next = foot + glm::ivec3(-2, 0, 0);
  placeBed(next, -90.0f, 1, ItemId::GREEN_BED);
  CHECK(at(next) == bedBlock(BedColour::Green, false, DoorFacing::North));
  CHECK(at(next + glm::ivec3(0, 0, -1)) == bedBlock(BedColour::Green, true, DoorFacing::North));
}

TEST_CASE_FIXTURE(BedGame, "a bed needs two clear cells with ground under both") {
  // Something where the head would go.
  game.world().setBlockAt(head(), BlockType::STONE);
  CHECK(reason(placeBed(foot)) == "place: no room for a bed");
  CHECK(at(foot) == BlockType::AIR);
  const PlayerState &me = *game.findPlayer(id);
  CHECK(me.inventory.slot(me.selected_slot) == ItemStack{ItemId::RED_BED, 1});
  // Water's no good either.
  game.world().setBlockAt(head(), BlockType::WATER);
  CHECK(reason(placeBed(foot)) == "place: no room for a bed");
  // A hole under the head.
  game.world().setBlockAt(head(), BlockType::AIR);
  game.world().setBlockAt(head() - glm::ivec3(0, 1, 0), BlockType::AIR);
  CHECK(reason(placeBed(foot)) == "place: a bed needs ground under it");
  CHECK(at(foot) == BlockType::AIR);
  // Not on top of another bed.
  game.world().setBlockAt(head() - glm::ivec3(0, 1, 0), BlockType::STONE);
  placeBed(foot);
  CHECK(reason(placeBed(foot + glm::ivec3(0, 1, 0))) == "place: a bed needs ground under it");
  // And a door won't stand on one.
  conn.send(MovePlayer{{5.5f, ground + 1.0f, 8.5f}, 0.0f, 0.0f, false, false});
  game.pump();
  game.findPlayer(id)->inventory.set(me.selected_slot, {ItemId::WOODEN_DOOR, 1});
  CHECK(reason(run(PlaceBlock{foot + glm::ivec3(0, 1, 0)})) ==
        "place: a door needs ground under it");
}

TEST_CASE_FIXTURE(BedGame, "breaking either half takes the whole bed and drops one of its colour") {
  for (bool hit_head : {false, true}) {
    CAPTURE(hit_head);
    placeBed(foot, 0.0f, 1, ItemId::BLUE_BED);
    PlayerState &me = *game.findPlayer(id);
    me.inventory.set(me.selected_slot, {});
    auto evs = mine(hit_head ? head() : foot);
    CHECK(changes(evs) == 2);
    CHECK(at(foot) == BlockType::AIR);
    CHECK(at(head()) == BlockType::AIR);
    CHECK(heldCount(inventory(), ItemId::BLUE_BED) == 1);
    for (int i = 0; i < kInventorySlots; ++i) me.inventory.set(i, {});
  }
}

TEST_CASE_FIXTURE(BedGame, "a bed pops off when the ground under either half goes") {
  for (bool under_head : {false, true}) {
    CAPTURE(under_head);
    placeBed(foot, 0.0f, 1, ItemId::PINK_BED);
    wield(ItemId::DIAMOND_PICKAXE);
    glm::ivec3 ground_cell = (under_head ? head() : foot) - glm::ivec3(0, 1, 0);
    auto evs = mine(ground_cell);
    CHECK(changes(evs) == 3);
    CHECK(at(foot) == BlockType::AIR);
    CHECK(at(head()) == BlockType::AIR);
    CHECK(dropped(game, ItemId::PINK_BED) == (under_head ? 2 : 1));
    game.world().setBlockAt(ground_cell, BlockType::STONE);
  }
}

TEST_CASE_FIXTURE(BedGame, "beds are saved") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string()));
  placeBed(foot, 0.0f, 1, ItemId::YELLOW_BED);
  run(Save{});

  ScopedRenderDistance rd{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  loaded.world().loadChunkSync(0, 0);
  loaded.world().flushPendingEdits();
  CHECK(loaded.world().getBlockAt(glm::vec3(foot) + 0.5f) ==
        bedBlock(BedColour::Yellow, false, DoorFacing::East));
  CHECK(loaded.world().getBlockAt(glm::vec3(head()) + 0.5f) ==
        bedBlock(BedColour::Yellow, true, DoorFacing::East));
}

// ─── Sleeping ──────────────────────────────────────────────────────────

namespace {

std::vector<std::string> notices(const std::vector<Event> &evs) {
  std::vector<std::string> out;
  for (const Event &e : evs)
    if (const auto *n = std::get_if<Notice>(&e)) out.push_back(n->text);
  return out;
}

std::optional<SleepState> lastSleep(const std::vector<Event> &evs) {
  std::optional<SleepState> out;
  for (const Event &e : evs)
    if (const auto *s = std::get_if<SleepState>(&e)) out = *s;
  return out;
}

std::optional<Respawned> lastRespawned(const std::vector<Event> &evs) {
  std::optional<Respawned> out;
  for (const Event &e : evs)
    if (const auto *r = std::get_if<Respawned>(&e)) out = *r;
  return out;
}

struct SleepGame : BedGame {
  ScopedDifficulty peaceful{Difficulty::Peaceful}; // nothing turns up to wake anyone
  glm::ivec3 second = foot + glm::ivec3(0, 0, 2); // another bed, for the other player

  SleepGame() {
    placeBed(second, 0.0f, 1, ItemId::BLUE_BED);
    placeBed(foot); // red, head to the east
    pollAll(other);
  }

  PlayerState &me() { return *game.findPlayer(id); }
  PlayerState &them() {
    for (PlayerId p = 1; p < 10; ++p)
      if (p != id && game.findPlayer(p)) return *game.findPlayer(p);
    FAIL("no second player");
    return me();
  }
  void night() { run(SetTimeOfDay{0.0f}); }
  std::vector<Event> ticks(int ms) {
    for (int t = 0; t < ms; t += Game::kTickMs) game.tick();
    return pollAll(conn);
  }
  // A move with the current respawn counter (sleeping bumps it).
  void walkTo(glm::vec3 pos) {
    conn.send(MovePlayer{pos, 0.0f, 0.0f, false, false, true, false, false, false,
                         me().respawn});
    game.pump();
  }
  // The observer walks over to the second bed and gets in.
  void otherSleeps() {
    other.send(MovePlayer{{5.5f, ground + 1.0f, 8.5f}, 0.0f, 0.0f, false, false});
    game.pump();
    other.send(UseBed{second});
    game.pump();
  }
};

} // namespace

TEST_CASE_FIXTURE(SleepGame, "by day a bed sets your respawn point but you can't sleep") {
  run(SetTimeOfDay{12 * 3600.0f});
  auto evs = run(UseBed{head()}); // either half
  CHECK(me().bed == foot);
  CHECK_FALSE(me().sleeping_in);
  CHECK(notices(evs) == std::vector<std::string>{"Respawn point set. You can only sleep at night"});
  // Already your bed: just the reason.
  CHECK(notices(run(UseBed{foot})) == std::vector<std::string>{"You can only sleep at night"});
}

TEST_CASE_FIXTURE(SleepGame, "at night you lie down on the bed and stay there") {
  night();
  auto evs = run(UseBed{foot});
  CHECK(notices(evs) == std::vector<std::string>{"Respawn point set"});
  REQUIRE(me().sleeping_in == foot);
  auto r = lastRespawned(evs);
  REQUIRE(r);
  CHECK(r->position == me().position);
  CHECK(me().position.y == doctest::Approx(ground + 1 + kBedHeight));
  CHECK(me().position.x == doctest::Approx(foot.x + 1.0f)); // between the halves
  CHECK(me().yaw == 0.0f);                                   // head to the east
  auto s = lastSleep(evs);
  REQUIRE(s);
  CHECK(s->in_bed);
  CHECK(s->bed == foot);
  CHECK(s->sleepers == 1);
  CHECK(s->living == 2);
  // The other player hears how many are in bed.
  auto theirs = lastSleep(pollAll(other));
  REQUIRE(theirs);
  CHECK_FALSE(theirs->in_bed);
  CHECK(theirs->sleepers == 1);

  // Moves (even with the new counter) are ignored, and so are edits.
  glm::vec3 where = me().position;
  conn.send(MovePlayer{{9.5f, ground + 1.0f, 9.5f}, 0.0f, 0.0f, false, false, true, false,
                       false, false, r->respawn});
  game.pump();
  CHECK(me().position == where);
  CHECK(reason(run(BreakBlock{foot + glm::ivec3(0, -1, -1)})) == "break: in bed");
  CHECK(reason(run(PlaceBlock{foot + glm::ivec3(0, 1, 0)})) == "place: in bed");
}

TEST_CASE_FIXTURE(SleepGame, "you can't sleep with a monster near, in a taken bed, or out of reach") {
  night();
  {
    ScopedDifficulty normal{Difficulty::Normal};
    auto e = game.entities().spawn(EntityKind::Gloomling, glm::vec3(foot) + glm::vec3(6.5f, 0, 0.5f));
    REQUIRE(e);
    CHECK(notices(run(UseBed{foot})) ==
          std::vector<std::string>{"Respawn point set. You can't sleep, monsters are nearby!"});
    CHECK_FALSE(me().sleeping_in);
    game.entities().find(*e)->body.position = glm::vec3(foot) + glm::vec3(12.5f, 0, 0.5f);
    run(UseBed{foot});
    CHECK(me().sleeping_in == foot); // far enough now
    game.entities().remove(*e);
  }
  // Someone's in it.
  other.send(MovePlayer{{5.5f, ground + 1.0f, 3.5f}, 0.0f, 0.0f, false, false});
  game.pump();
  pollAll(other);
  other.send(UseBed{foot});
  game.pump();
  CHECK(notices(pollAll(other)) == std::vector<std::string>{"Respawn point set. This bed is taken"});
  CHECK_FALSE(them().sleeping_in);

  run(LeaveBed{});
  walkTo({5.5f, ground + 1.0f, 5.5f + 9.0f});
  CHECK(reason(run(UseBed{foot})) == "bed: out of reach");
  CHECK(reason(run(UseBed{foot + glm::ivec3(0, -1, 0)})) == "bed: that isn't a bed");
}

TEST_CASE_FIXTURE(SleepGame, "the night skips only once every living player is in bed") {
  night();
  run(UseBed{foot});
  ticks(4000);
  CHECK(me().sleeping_in); // still waiting for the other player
  CHECK(game.clock().time_of_day < 3600.0f);

  otherSleeps();
  REQUIRE(them().sleeping_in);
  ticks(1000);
  CHECK(game.clock().time_of_day < 3600.0f); // not long enough yet
  auto evs = ticks(1100);
  CHECK(game.clock().time_of_day == doctest::Approx(Game::kWakeTime).epsilon(0.001));
  CHECK_FALSE(me().sleeping_in);
  CHECK_FALSE(them().sleeping_in);
  auto s = lastSleep(evs);
  REQUIRE(s);
  CHECK_FALSE(s->in_bed);
  CHECK(s->sleepers == 0);
  // Up beside the bed, on the floor.
  auto r = lastRespawned(evs);
  REQUIRE(r);
  CHECK(r->position.y == doctest::Approx(ground + 1.0f));
  CHECK(at(glm::ivec3(glm::floor(r->position))) == BlockType::AIR);
}

TEST_CASE_FIXTURE(SleepGame, "a dead player doesn't keep everyone up") {
  night();
  run(UseBed{foot});
  them().vitals.damage(1000);
  REQUIRE(them().vitals.dead());
  ticks(2100);
  CHECK(game.clock().time_of_day == doctest::Approx(Game::kWakeTime).epsilon(0.001));
  CHECK_FALSE(me().sleeping_in);
}

TEST_CASE_FIXTURE(SleepGame, "getting up, getting hurt, or losing the bed wakes you") {
  night();
  SUBCASE("getting up") {
    run(UseBed{foot});
    auto evs = run(LeaveBed{});
    CHECK_FALSE(me().sleeping_in);
    REQUIRE(lastRespawned(evs));
    CHECK(lastRespawned(evs)->position.y == doctest::Approx(ground + 1.0f));
  }
  SUBCASE("getting hurt") {
    run(UseBed{foot});
    me().vitals.damage(2);
    ticks(Game::kTickMs);
    CHECK_FALSE(me().sleeping_in);
  }
  SUBCASE("the bed broken under you") {
    otherSleeps();
    REQUIRE(them().sleeping_in);
    REQUIRE(them().sleeping_in == second);
    me().inventory.set(me().selected_slot, {});
    mine(second);
    CHECK(at(second) == BlockType::AIR);
    ticks(Game::kTickMs);
    CHECK_FALSE(them().sleeping_in);
  }
  // Morning without the skip wakes you too.
  run(UseBed{foot});
  run(SetTimeOfDay{7 * 3600.0f});
  ticks(Game::kTickMs);
  CHECK_FALSE(me().sleeping_in);
}

TEST_CASE_FIXTURE(SleepGame, "you respawn beside your bed, or at spawn if it's gone") {
  run(UseBed{foot}); // by day: just the respawn point
  REQUIRE(me().bed == foot);
  me().vitals.damage(1000);
  auto evs = ticks(Game::kRespawnDelayMs + 100);
  auto r = lastRespawned(evs);
  REQUIRE(r);
  CHECK(glm::distance(r->position, glm::vec3(foot) + glm::vec3(1.0f, 0, 0.5f)) < 2.0f);
  CHECK(r->position.y == doctest::Approx(ground + 1.0f));

  // Break it, die again: back to the world spawn.
  walkTo({5.5f, ground + 1.0f, 8.5f});
  me().inventory.set(me().selected_slot, {});
  mine(foot);
  REQUIRE(at(foot) == BlockType::AIR);
  me().vitals.damage(1000);
  evs = ticks(Game::kRespawnDelayMs + 100);
  r = lastRespawned(evs);
  REQUIRE(r);
  CHECK(r->position == game.world().findSpawn());
  CHECK_FALSE(me().bed);
  CHECK(notices(evs) == std::vector<std::string>{"Your bed is missing"});
}

TEST_CASE_FIXTURE(SleepGame, "the respawn bed is saved, for the host and for a guest") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string()));
  run(UseBed{foot});
  REQUIRE(me().bed == foot);
  run(Save{});

  ScopedRenderDistance rd{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  LocalConnection again;
  Joined j = join(loaded, again);
  CHECK(loaded.findPlayer(j.id)->bed == foot);

  // A guest's record carries theirs.
  WorldSave::GuestRecord rec;
  rec.player.bed = glm::ivec3(-3, 70, 12);
  REQUIRE(WorldSave::saveGuest(dir.path.string(), "Aria", rec));
  std::map<std::string, WorldSave::GuestRecord> guests;
  REQUIRE(WorldSave::loadGuests(dir.path.string(), guests));
  CHECK(guests.at("Aria").player.bed == glm::ivec3(-3, 70, 12));
}
