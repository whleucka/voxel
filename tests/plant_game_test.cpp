// Plants, torches, cactus and cake in the Game: placing, popping, picking,
// eating and pricking, through a LocalConnection as Engine drives it.
#include "doctest/doctest.h"
#include "biome/biome_manager.hpp"
#include "biome/plants.hpp"
#include "block/block_data.hpp"
#include "item/mining.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include <algorithm>

namespace {

constexpr int kBase = 200; // the garden's grass: clear sky above

std::vector<Event> pollAll(LocalConnection &conn) {
  std::vector<Event> out;
  conn.poll(out);
  return out;
}

template <class T> std::optional<T> find(const std::vector<Event> &evs) {
  for (const Event &e : evs)
    if (const T *t = std::get_if<T>(&e)) return *t;
  return std::nullopt;
}

std::string reason(const std::vector<Event> &evs) {
  auto r = find<CommandRejected>(evs);
  return r ? r->reason : "";
}

// A garden in the sky over chunk (0,0): grass on dirt from x/z 0 to 15, with
// a strip of sand along x 6..15. Two players stand in its corner.
struct Garden {
  ScopedRenderDistance rd{0};
  Game game;
  LocalConnection a, b;
  PlayerId ida = 0, idb = 0;
  static inline const glm::vec3 kStand{2.5f, kBase + 1.0f, 2.5f};

  Garden() {
    game.spawner() = Spawner{};
    ida = join(a);
    idb = join(b);
    game.world().loadChunkSync(0, 0);
    fill(game.world(), {0, kBase - 3, 0}, {15, kBase - 1, 15}, BlockType::DIRT);
    fill(game.world(), {0, kBase, 0}, {15, kBase, 15}, BlockType::GRASS);
    fill(game.world(), {6, kBase, 0}, {15, kBase, 15}, BlockType::SAND);
    fill(game.world(), {0, kBase + 1, 0}, {15, kBase + 12, 15}, BlockType::AIR);
    stand(a, ida, kStand);
    stand(b, idb, kStand + glm::vec3(0, 0, 1));
  }

  PlayerId join(LocalConnection &conn) {
    game.addClient(conn);
    conn.send(Join{});
    game.pump();
    auto joined = find<Joined>(pollAll(conn));
    REQUIRE(joined);
    return joined->id;
  }

  PlayerState &ps(PlayerId id) { return *game.findPlayer(id); }

  std::vector<Event> run(LocalConnection &conn, Command cmd) {
    conn.send(std::move(cmd));
    game.pump();
    return pollAll(conn);
  }
  std::vector<Event> run(Command cmd) { return run(a, std::move(cmd)); }

  void stand(LocalConnection &conn, PlayerId id, glm::vec3 at) {
    MovePlayer m;
    m.position = at;
    m.on_ground = true;
    m.respawn = ps(id).respawn;
    run(conn, m);
  }

  void hold(ItemId item, uint16_t n = 1, PlayerId id = 0) {
    PlayerState &p = ps(id ? id : ida);
    p.inventory.set(p.selected_slot, {item, n});
  }
  void hold(BlockType b, uint16_t n = 1) { hold(itemForBlock(b), n); }

  BlockType at(glm::ivec3 p) const { return game.world().getBlockAt(glm::vec3(p) + 0.5f); }
  void put(glm::ivec3 p, BlockType b) { game.world().setBlockAt(p, b); }

  // Breaks a block as a survival player does: start, keep at it, break.
  std::vector<Event> mine(glm::ivec3 pos) {
    run(StartBreak{pos});
    PlayerState &me = ps(ida);
    int ms = breakMs(at(pos), me.inventory.slot(me.selected_slot).id);
    for (int t = 0; t < ms; t += Game::kTickMs) game.tick();
    pollAll(a);
    return run(BreakBlock{pos});
  }

  int dropped(ItemId id) const {
    int n = 0;
    for (const auto &[eid, e] : game.entities().all())
      if (e.kind == EntityKind::DroppedItem && e.item.id == id) n += e.item.count;
    return n;
  }
  int have(ItemId id, PlayerId who = 0) {
    int n = 0;
    for (const ItemStack &s : ps(who ? who : ida).inventory.slots())
      if (s.id == id) n += s.count;
    return n;
  }
};

const glm::ivec3 kUp{0, 1, 0};
const glm::ivec3 kGrass{4, kBase, 4}; // in reach of the corner
const glm::ivec3 kSand{6, kBase, 3};

} // namespace

// ─── Placing ───────────────────────────────────────────────────────────

TEST_CASE_FIXTURE(Garden, "a flower goes on grass or dirt, not on stone or sand") {
  hold(BlockType::POPPY, 3);
  CHECK(reason(run(PlaceBlock{kGrass + kUp})).empty());
  CHECK(at(kGrass + kUp) == BlockType::POPPY);

  put(kGrass + glm::ivec3(1, 0, 0), BlockType::STONE);
  CHECK(reason(run(PlaceBlock{kGrass + glm::ivec3(1, 1, 0)})) == "place: it can't go there");
  CHECK(reason(run(PlaceBlock{kSand + kUp})) == "place: it can't go there");
  // Not floating either.
  CHECK(reason(run(PlaceBlock{kGrass + glm::ivec3(0, 3, 1)})) == "place: it can't go there");
}

TEST_CASE_FIXTURE(Garden, "a flower can go where someone stands: it's walked through") {
  const glm::ivec3 feet(glm::floor(kStand));
  hold(BlockType::DAISY);
  CHECK(reason(run(PlaceBlock{feet})).empty());
  CHECK(at(feet) == BlockType::DAISY);
  hold(BlockType::DIRT);
  CHECK(reason(run(PlaceBlock{feet + glm::ivec3(0, 1, 0)})) == "place: inside a player");
}

TEST_CASE_FIXTURE(Garden, "cactus, dead bushes and shells go on sand; mushrooms on any solid block") {
  hold(BlockType::CACTUS, 2);
  CHECK(reason(run(PlaceBlock{kGrass + kUp})) == "place: it can't go there");
  CHECK(reason(run(PlaceBlock{kSand + kUp})).empty());
  CHECK(reason(run(PlaceBlock{kSand + kUp + kUp})).empty()); // stacked
  CHECK(at(kSand + kUp + kUp) == BlockType::CACTUS);

  hold(BlockType::DEAD_BUSH);
  CHECK(reason(run(PlaceBlock{kSand + glm::ivec3(0, 1, 1)})).empty());

  put(kGrass + glm::ivec3(1, 0, 0), BlockType::STONE);
  hold(BlockType::BROWN_MUSHROOM);
  CHECK(reason(run(PlaceBlock{kGrass + glm::ivec3(1, 1, 0)})).empty());
}

TEST_CASE_FIXTURE(Garden, "a torch goes on the floor or the side of a block, not a ceiling") {
  hold(BlockType::TORCH, 4);
  CHECK(reason(run(PlaceBlock{kGrass + kUp, {0, -1, 0}})).empty());
  CHECK(at(kGrass + kUp) == BlockType::TORCH);

  // A wall east of (5, kBase + 2, 3): clicking its west face hangs a torch on it.
  const glm::ivec3 wall{6, kBase + 2, 3};
  put(wall, BlockType::STONE);
  CHECK(reason(run(PlaceBlock{wall - glm::ivec3(1, 0, 0), {1, 0, 0}})).empty());
  CHECK(at(wall - glm::ivec3(1, 0, 0)) == BlockType::TORCH_E);

  // Under a ceiling, or against nothing, it's refused.
  put(wall + glm::ivec3(0, 0, 1) + kUp, BlockType::STONE);
  CHECK(reason(run(PlaceBlock{wall + glm::ivec3(0, 0, 1), {0, 1, 0}})) ==
        "place: torches go on floors and walls");
  CHECK(reason(run(PlaceBlock{wall + glm::ivec3(0, 0, 2), {1, 0, 0}})) ==
        "place: it can't go there");
}

// ─── Popping ───────────────────────────────────────────────────────────

TEST_CASE_FIXTURE(Garden, "break the ground under a flower and the flower pops off") {
  game.setMode(GameMode::Survival);
  put(kGrass + kUp, BlockType::CORNFLOWER);
  hold(ItemId::STONE_SHOVEL);
  mine(kGrass);
  CHECK(at(kGrass) == BlockType::AIR);
  CHECK(at(kGrass + kUp) == BlockType::AIR);
  CHECK(dropped(itemForBlock(BlockType::CORNFLOWER)) == 1);
}

TEST_CASE_FIXTURE(Garden, "a torch pops off the wall that goes, and only that wall") {
  game.setMode(GameMode::Survival);
  const glm::ivec3 wall{5, kBase + 1, 4};
  put(wall, BlockType::DIRT);
  put(wall + glm::ivec3(0, 0, 1), BlockType::TORCH_N); // on the wall's south face
  put(wall + glm::ivec3(-1, 0, 0), BlockType::TORCH);  // on the floor beside it
  hold(ItemId::STONE_SHOVEL);
  mine(wall);
  CHECK(at(wall + glm::ivec3(0, 0, 1)) == BlockType::AIR);
  CHECK(at(wall + glm::ivec3(-1, 0, 0)) == BlockType::TORCH);
  CHECK(dropped(itemForBlock(BlockType::TORCH)) == 1);
}

TEST_CASE_FIXTURE(Garden, "a cactus stack falls all the way up when its sand goes") {
  game.setMode(GameMode::Survival);
  for (int y = 1; y <= 3; ++y) put(kSand + glm::ivec3(0, y, 0), BlockType::CACTUS);
  hold(ItemId::STONE_SHOVEL);
  mine(kSand);
  for (int y = 1; y <= 3; ++y) CHECK(at(kSand + glm::ivec3(0, y, 0)) == BlockType::AIR);
  CHECK(dropped(itemForBlock(BlockType::CACTUS)) == 3);
}

TEST_CASE_FIXTURE(Garden, "tall grass now and then gives a wheat seed, never itself") {
  game.setMode(GameMode::Survival);
  hold(ItemId::NONE, 0);
  int broken = 0;
  for (int z = 1; z <= 6; ++z)
    for (int x = 1; x <= 6; ++x) {
      glm::ivec3 cell{x, kBase + 1, z};
      if (cell == glm::ivec3(glm::floor(kStand))) continue;
      put(cell, BlockType::TALL_GRASS);
      mine(cell);
      ++broken;
    }
  const int seeds = have(ItemId::WHEAT_SEEDS);
  CHECK(seeds > 0);
  CHECK(seeds < broken / 2);
  CHECK(have(itemForBlock(BlockType::TALL_GRASS)) == 0);
}

TEST_CASE_FIXTURE(Garden, "a dead bush gives a stick, shells a sea shell, a cake nothing") {
  game.setMode(GameMode::Survival);
  hold(ItemId::NONE, 0);
  put(kSand + kUp, BlockType::DEAD_BUSH);
  mine(kSand + kUp);
  CHECK(have(ItemId::STICKS) == 1);
  put(kSand + kUp, BlockType::SHELLS);
  mine(kSand + kUp);
  CHECK(have(ItemId::SEA_SHELL) == 1);
  put(kGrass + kUp, BlockType::CAKE_3);
  mine(kGrass + kUp);
  CHECK(at(kGrass + kUp) == BlockType::AIR);
  CHECK(have(itemForBlock(BlockType::CAKE_4)) == 0);
}

TEST_CASE_FIXTURE(Garden, "picking a flower doesn't wear the tool in hand") {
  game.setMode(GameMode::Survival);
  hold(ItemId::STONE_SWORD);
  put(kGrass + kUp, BlockType::POPPY);
  mine(kGrass + kUp);
  CHECK(ps(ida).inventory.slot(ps(ida).selected_slot).wear == 0);
}

// ─── Berry bushes ──────────────────────────────────────────────────────

TEST_CASE_FIXTURE(Garden, "a ripe berry bush gives two berries and ripens again in five minutes") {
  const glm::ivec3 bush = kGrass + kUp;
  put(bush, BlockType::BERRY_BUSH);
  hold(ItemId::NONE, 0);
  CHECK(reason(run(UseItemOn{bush})).empty());
  CHECK(have(ItemId::BERRIES) == Game::kBerriesPicked);
  CHECK(at(bush) == BlockType::BERRY_BUSH_BARE);
  CHECK(game.cropTimeLeft(bush) == Game::kBushRipenMs);

  // Bare, it has nothing to give yet.
  auto evs = run(UseItemOn{bush});
  REQUIRE(find<Notice>(evs));
  CHECK(have(ItemId::BERRIES) == Game::kBerriesPicked);

  game.growFarms(Game::kBushRipenMs - 1000);
  CHECK(at(bush) == BlockType::BERRY_BUSH_BARE);
  game.growFarms(1000);
  CHECK(at(bush) == BlockType::BERRY_BUSH);
  CHECK_FALSE(game.cropTimeLeft(bush));
}

TEST_CASE_FIXTURE(Garden, "a picked bush's time is saved with the farms") {
  ScopedSeed seed;
  TempDir dir;
  CHECK_FALSE(game.load(dir.path.string()));
  const glm::ivec3 bush = kGrass + kUp;
  put(bush, BlockType::BERRY_BUSH);
  run(UseItemOn{bush});
  game.growFarms(60'000);
  run(Save{});

  ScopedRenderDistance rd2{0};
  Game loaded;
  REQUIRE(loaded.load(dir.path.string()));
  CHECK(loaded.cropTimeLeft(bush) == Game::kBushRipenMs - 60'000);
  // It ripens where nobody is, too.
  loaded.growFarms(Game::kBushRipenMs);
  loaded.world().loadChunkSync(0, 0);
  loaded.world().flushPendingEdits();
  CHECK(loaded.world().getBlockAt(glm::vec3(bush) + 0.5f) == BlockType::BERRY_BUSH);
}

TEST_CASE_FIXTURE(Garden, "a bare bush nobody was timing starts ripening when tried") {
  const glm::ivec3 bush = kGrass + kUp;
  put(bush, BlockType::BERRY_BUSH_BARE);
  CHECK_FALSE(game.cropTimeLeft(bush));
  run(UseItemOn{bush});
  CHECK(game.cropTimeLeft(bush) == Game::kBushRipenMs);
}

// ─── Cake ──────────────────────────────────────────────────────────────

TEST_CASE_FIXTURE(Garden, "a cake is shared a slice at a time, by whoever is hungry") {
  game.setMode(GameMode::Survival);
  const glm::ivec3 cake = kGrass + kUp;
  hold(BlockType::CAKE_4);
  REQUIRE(reason(run(PlaceBlock{cake})).empty());
  CHECK(at(cake) == BlockType::CAKE_4);

  // Full up: no thanks.
  ps(ida).vitals.reset();
  ps(ida).vitals.feed(Vitals::kMaxFedMs / Vitals::kFedPerPointMs + 1);
  CHECK(reason(run(UseItemOn{cake})) == "eat: not hungry");
  CHECK(at(cake) == BlockType::CAKE_4);

  ps(ida).vitals.damage(10);
  ps(idb).vitals.damage(10);
  const int a_before = ps(ida).vitals.health();
  CHECK(reason(run(UseItemOn{cake})).empty());
  CHECK(ps(ida).vitals.health() == a_before + Game::kCakeSliceHeal);
  CHECK(at(cake) == BlockType::CAKE_3);

  // The other player has some too, then it's finished between them.
  CHECK(reason(run(b, UseItemOn{cake})).empty());
  CHECK(at(cake) == BlockType::CAKE_2);
  run(UseItemOn{cake});
  run(b, UseItemOn{cake});
  CHECK(at(cake) == BlockType::AIR);
}

// ─── Compost ───────────────────────────────────────────────────────────

TEST_CASE_FIXTURE(Garden, "compost grows a crop one stage and is used up") {
  game.setMode(GameMode::Survival);
  put(kGrass, BlockType::FARMLAND_WET);
  put(kGrass + kUp, BlockType::WHEAT_0);
  hold(ItemId::COMPOST, 2);
  CHECK(reason(run(UseItemOn{kGrass + kUp})).empty());
  CHECK(at(kGrass + kUp) == BlockType::WHEAT_1);
  CHECK(have(ItemId::COMPOST) == 1);
  CHECK(game.cropTimeLeft(kGrass + kUp).has_value());
}

TEST_CASE_FIXTURE(Garden, "compost ripens a picked bush at once") {
  put(kGrass + kUp, BlockType::BERRY_BUSH);
  run(UseItemOn{kGrass + kUp});
  hold(ItemId::COMPOST);
  run(UseItemOn{kGrass + kUp});
  CHECK(at(kGrass + kUp) == BlockType::BERRY_BUSH);
  CHECK_FALSE(game.cropTimeLeft(kGrass + kUp));
}

TEST_CASE_FIXTURE(Garden, "compost on grass sprouts the biome's plants round it") {
  const BiomeType biome = BiomeManager::getBiomeForChunk(0, 0);
  const plants::Wild &wild = plants::wildFor(biome);
  hold(ItemId::COMPOST);
  auto evs = run(UseItemOn{kGrass});
  if (wild.greens.empty() && wild.flowers.empty()) {
    CHECK(reason(evs) == "compost: nothing grows here");
    return;
  }
  CHECK(reason(evs).empty());
  int sprouted = 0;
  for (int dz = -Game::kCompostReach; dz <= Game::kCompostReach; ++dz)
    for (int dx = -Game::kCompostReach; dx <= Game::kCompostReach; ++dx) {
      BlockType b = at(kGrass + glm::ivec3(dx, 1, dz));
      if (b == BlockType::AIR) continue;
      CAPTURE(blockName(b));
      bool wild_here =
          std::find(wild.greens.begin(), wild.greens.end(), b) != wild.greens.end() ||
          std::find(wild.flowers.begin(), wild.flowers.end(), b) != wild.flowers.end();
      CHECK(wild_here);
      ++sprouted;
    }
  CHECK(sprouted >= 1);
  CHECK(sprouted <= Game::kCompostSprouts);
}

TEST_CASE_FIXTURE(Garden, "compost on stone does nothing and isn't used") {
  put(kGrass, BlockType::STONE);
  hold(ItemId::COMPOST);
  CHECK(reason(run(UseItemOn{kGrass})) == "compost: on a crop, a bush, grass or sand");
  CHECK(have(ItemId::COMPOST) == 1);
}

// ─── Cactus ────────────────────────────────────────────────────────────

TEST_CASE_FIXTURE(Garden, "touching a cactus pricks for one point, at most once a second") {
  game.setMode(GameMode::Survival);
  const glm::ivec3 feet(glm::floor(kStand));
  put(feet + glm::ivec3(1, 0, 0), BlockType::CACTUS);
  game.tick();
  CHECK(ps(ida).vitals.health() == Vitals::kMax); // a step away: fine

  stand(a, ida, kStand + glm::vec3(0.5f - kPlayerHalfWidth, 0, 0)); // leaning on it
  const int full = ps(ida).vitals.health();
  game.tick();
  CHECK(ps(ida).vitals.health() == full - Game::kPrickDamage);
  for (int t = 0; t < Game::kPrickCooldownMs / Game::kTickMs - 1; ++t) game.tick();
  CHECK(ps(ida).vitals.health() == full - Game::kPrickDamage);
  game.tick();
  CHECK(ps(ida).vitals.health() == full - 2 * Game::kPrickDamage);
  CHECK(ps(ida).hurt_by == "was pricked by a cactus");
}

TEST_CASE_FIXTURE(Garden, "a cactus doesn't prick in creative") {
  game.setMode(GameMode::Creative);
  const glm::ivec3 feet(glm::floor(kStand));
  put(feet + glm::ivec3(1, 0, 0), BlockType::CACTUS);
  stand(a, ida, kStand + glm::vec3(0.5f - kPlayerHalfWidth, 0, 0));
  for (int t = 0; t < 40; ++t) game.tick();
  CHECK(ps(ida).vitals.health() == Vitals::kMax);
}
