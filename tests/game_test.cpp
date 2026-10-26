// Game is exercised through a LocalConnection exactly as Engine drives it,
// without a window: World::init() (shaders) is never called.
#include "doctest/doctest.h"
#include "core/settings.hpp"
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
  REQUIRE(events.size() == 2);
  REQUIRE(std::holds_alternative<Joined>(events[0]));
  REQUIRE(std::holds_alternative<InventoryContents>(events[1]));
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

  // Places a block of the given type the way a player would: hold it, place.
  std::vector<Event> place(glm::ivec3 pos, BlockType b) {
    hold(b);
    return run(PlaceBlock{pos});
  }
};

} // namespace

TEST_CASE_FIXTURE(EditGame, "a valid break removes the block and broadcasts once") {
  glm::ivec3 target{5, ground, 5};
  auto evs = run(BreakBlock{target});

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
  CHECK(rejected(run(BreakBlock{bedrock})));
  CHECK(at(bedrock) == BlockType::BEDROCK);

  glm::ivec3 water{5, ground + 2, 5};
  game.world().setBlockAt(water, BlockType::WATER);
  CHECK(rejected(run(BreakBlock{water})));
  CHECK(at(water) == BlockType::WATER);

  CHECK(rejected(run(BreakBlock{{5, ground + 3, 5}})));
  CHECK(pollAll(other).empty());
}

TEST_CASE_FIXTURE(EditGame, "edits out of reach are rejected") {
  glm::ivec3 far{5, ground, 5};
  moveTo({5.5f, ground + 1.0f, 5.5f + 8.0f});
  CHECK(rejected(run(BreakBlock{far})));
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

  REQUIRE(changes(run(BreakBlock{lamp})) == 1);
  CHECK(game.world().getBlockLightAt(lamp + glm::ivec3(1, 0, 0)) == 0);
}

// ─── Save / load ───────────────────────────────────────────────────────

TEST_CASE_FIXTURE(EditGame, "a played game round-trips through save and load") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string())); // empty dir: fresh world

  glm::ivec3 broken{5, ground, 5};
  glm::ivec3 placed{6, ground + 1, 5};
  REQUIRE(at(placed) == BlockType::AIR);
  REQUIRE(changes(run(BreakBlock{broken})) == 1);
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

  auto evs = run(BreakBlock{target});
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
  run(BreakBlock{ore});
  CHECK(countOf(inventory(), ItemId::DIAMOND) == 1);
  CHECK(countOf(inventory(), itemForBlock(BlockType::DIAMOND_ORE)) == 0);
}

TEST_CASE_FIXTURE(EditGame, "with a full inventory the block breaks and the drop is lost") {
  run(GiveItem{itemForBlock(BlockType::SAND), 64 * kInventorySlots});
  // Every slot but the kit's glowstone is now a full stack of sand.
  REQUIRE(countOf(inventory(), itemForBlock(BlockType::SAND)) ==
          64 * (kInventorySlots - 1));
  Inventory before = inventory();

  glm::ivec3 target{5, ground + 1, 5};
  game.world().setBlockAt(target, BlockType::COBBLESTONE);
  CHECK(changes(run(BreakBlock{target})) == 1);
  CHECK(at(target) == BlockType::AIR);
  CHECK(inventory().slots() == before.slots());
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
