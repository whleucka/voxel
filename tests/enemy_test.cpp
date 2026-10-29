#include "doctest/doctest.h"
#include "core/constants.hpp"
#include "block/block_type.hpp"
#include "item/item.hpp"
#include "entity/brain.hpp"
#include "entity/entity_store.hpp"
#include "test_util.hpp"
#include "world/world.hpp"

namespace {

constexpr int kBase = 200; // clear sky above the generated terrain

// One chasing creature and one player on a hand-built rig in the sky.
struct ChaseRig {
  World world;
  EntityStore store;
  Rng rng{31};
  std::vector<glm::vec3> players;
  EntityId id = 0;

  ChaseRig() {
    for (int cx = 0; cx < 3; ++cx) world.loadChunkSync(cx, 0);
    fill(world, {1, kBase, 1}, {46, kBase, 14}, BlockType::STONE);
  }
  void spawn(glm::vec3 at) {
    auto e = store.spawn(EntityKind::TestCreature, at);
    REQUIRE(e);
    id = *e;
    store.find(id)->brain = std::make_unique<ChaseBrain>();
  }
  const Entity &e() const { return *store.find(id); }
  const ChaseBrain &brain() const { return static_cast<const ChaseBrain &>(*e().brain); }
  float gap() const {
    glm::vec3 p = e().body.position, t = players.front();
    return glm::length(glm::vec2(p.x - t.x, p.z - t.z));
  }
  // Ticks until the creature is within `reach` of the player; false on timeout.
  bool reaches(float reach, int max_ticks) {
    for (int i = 0; i < max_ticks; ++i) {
      store.tick(world, rng, players, players);
      if (gap() < reach) return true;
    }
    return false;
  }
};

} // namespace

TEST_CASE("a chaser reaches a player 10 blocks away on flat ground") {
  ChaseRig r;
  r.players = {{14.5f, kBase + 1.0f, 8.5f}};
  r.spawn({4.5f, kBase + 1.0f, 8.5f});
  CHECK(r.reaches(1.0f, 20 * 10));
  CHECK(r.brain().chasing());
}

TEST_CASE("a chaser that reaches the player stands touching them, facing them, not spinning") {
  ChaseRig r;
  r.players = {{14.5f, kBase + 1.0f, 8.5f}};
  r.spawn({4.5f, kBase + 1.0f, 8.5f});
  REQUIRE(r.reaches(1.0f, 20 * 10));
  const float touching = r.e().body.half_width + kPlayerHalfWidth;
  for (int i = 0; i < 40; ++i) {
    r.store.tick(r.world, r.rng, r.players, r.players);
    CAPTURE(i);
    // Close enough to hit (the Game's contact range is 0.3), but not inside.
    CHECK(r.gap() >= touching - 0.05f);
    CHECK(r.gap() <= touching + 0.3f);
    // Facing the player: they're at +X, a yaw of 90.
    CHECK(r.e().yaw == doctest::Approx(90.0f).epsilon(0.1));
  }
}

TEST_CASE("a chaser finds the gap in a wall") {
  ChaseRig r;
  // A wall across x = 8, open only at z = 13.
  fill(r.world, {8, kBase + 1, 1}, {8, kBase + 3, 12}, BlockType::STONE);
  r.players = {{12.5f, kBase + 1.0f, 4.5f}};
  r.spawn({4.5f, kBase + 1.0f, 4.5f});
  CHECK(r.reaches(1.0f, 20 * 20));
}

TEST_CASE("a chaser jumps up a step to get to the player") {
  ChaseRig r;
  fill(r.world, {9, kBase + 1, 1}, {46, kBase + 1, 14}, BlockType::STONE);
  r.players = {{14.5f, kBase + 2.0f, 8.5f}};
  r.spawn({4.5f, kBase + 1.0f, 8.5f});
  CHECK(r.reaches(1.0f, 20 * 10));
  CHECK(r.e().body.position.y > kBase + 1.5f);
}

TEST_CASE("with no player in range a chaser wanders") {
  ChaseRig r;
  r.players = {{45.5f, kBase + 1.0f, 8.5f}}; // beyond the 24-block sense range
  r.spawn({8.5f, kBase + 1.0f, 8.5f});
  glm::vec3 start = r.e().body.position;
  float furthest = 0.0f;
  for (int i = 0; i < 20 * 30; ++i) {
    r.store.tick(r.world, r.rng, r.players, r.players);
    REQUIRE_FALSE(r.brain().chasing());
    furthest = std::max(furthest, glm::distance(start, r.e().body.position));
  }
  CHECK(furthest > 1.0f);
  CHECK(r.e().body.position.y > kBase + 0.5f); // cautious: it stays on the rig
}

TEST_CASE("a chaser ignores players it isn't given as targets") {
  ChaseRig r;
  r.players = {{14.5f, kBase + 1.0f, 8.5f}};
  r.spawn({4.5f, kBase + 1.0f, 8.5f});
  for (int i = 0; i < 20; ++i) r.store.tick(r.world, r.rng, r.players, {});
  CHECK_FALSE(r.brain().chasing());
}

// ─── Enemy kinds ───────────────────────────────────────────────────────

TEST_CASE("the enemies match the enemy table") {
  struct Row {
    EntityKind kind;
    float width, height, speed;
    int health, damage;
    bool surface, vanishes;
    uint16_t glowstone;
  };
  const Row rows[] = {
      {EntityKind::Gloomling, 0.6f, 0.6f, 3.0f, 6, 1, true, true, 1},
      {EntityKind::ThornCrawler, 0.7f, 0.5f, 4.2f, 4, 2, true, true, 1},
      {EntityKind::CaveGrumble, 0.8f, 1.3f, 1.6f, 16, 2, false, false, 2},
      {EntityKind::Zombie, 0.6f, 1.8f, 2.2f, 10, 2, true, true, 1},
      {EntityKind::Mummy, 0.6f, 1.8f, 1.8f, 12, 2, true, false, 1},
  };
  REQUIRE(std::size(kEnemyKinds) == std::size(rows));
  for (const Row &row : rows) {
    CAPTURE(static_cast<int>(row.kind));
    REQUIRE(isEnemy(row.kind));
    CHECK_FALSE(isAnimal(row.kind));
    const EntityTypeDef &d = entityType(row.kind);
    CHECK(d.half_width * 2.0f == doctest::Approx(row.width));
    CHECK(d.height == doctest::Approx(row.height));
    CHECK(d.walk_speed == doctest::Approx(row.speed));
    CHECK(d.max_health == row.health); // > 0: can be hit
    CHECK(d.enemy->damage == row.damage);
    CHECK(d.enemy->surface == row.surface);
    CHECK(d.enemy->vanish_at_dawn == row.vanishes);
    CHECK(d.brain);
    REQUIRE(d.drops.size() == 1);
    CHECK(d.drops[0] == ItemStack{itemForBlock(BlockType::GLOWSTONE), row.glowstone});
    REQUIRE(d.chance_drops.size() >= 1);
    CHECK(d.chance_drops[0].stack == ItemStack{ItemId::COAL, 1});
    CHECK(d.chance_drops[0].chance == doctest::Approx(0.5f));

    int glowing = 0;
    for (const ModelBox &b : d.model) glowing += b.glow;
    CHECK(glowing == 2); // a pair of eyes
  }
  CHECK(entityType(EntityKind::ThornCrawler).enemy->biomes.size() == 3);
  CHECK(entityType(EntityKind::Gloomling).enemy->biomes.empty());
}

TEST_CASE("zombies sometimes drop a carrot, mummies sometimes wool") {
  const auto &zombie = entityType(EntityKind::Zombie).chance_drops;
  REQUIRE(zombie.size() == 2);
  CHECK(zombie[1].stack == ItemStack{ItemId::CARROT, 1});
  CHECK(zombie[1].chance == doctest::Approx(0.25f));
  const auto &mummy = entityType(EntityKind::Mummy).chance_drops;
  REQUIRE(mummy.size() == 2);
  CHECK(mummy[1].stack == ItemStack{ItemId::WOOL, 1});
  CHECK(mummy[1].chance == doctest::Approx(0.5f));
}

TEST_CASE("zombies roam the grassy lands, mummies only the desert") {
  using B = BiomeType;
  CHECK(entityType(EntityKind::Zombie).enemy->biomes ==
        std::vector<B>{B::PLAINS, B::MOUNTAIN, B::CHERRY, B::TROPICAL});
  CHECK(entityType(EntityKind::Mummy).enemy->biomes == std::vector<B>{B::DESERT});
}

TEST_CASE("animals and other kinds aren't enemies") {
  for (EntityKind k : kAnimalKinds) CHECK_FALSE(isEnemy(k));
  CHECK_FALSE(isEnemy(EntityKind::TestCreature));
  CHECK_FALSE(isEnemy(EntityKind::DroppedItem));
  CHECK_FALSE(isEnemy(static_cast<EntityKind>(999)));
}

TEST_CASE("enemies chase") {
  EntityStore store;
  for (EntityKind k : kEnemyKinds) {
    auto id = store.spawn(k, {0, 100, 0});
    REQUIRE(id);
    CHECK(dynamic_cast<ChaseBrain *>(store.find(*id)->brain.get()));
  }
}

// ─── Spawning ──────────────────────────────────────────────────────────

#include "entity/enemies.hpp"
#include "entity/spawner.hpp"

namespace {

constexpr float kMidnight = 0.0f, kNoon = 0.5f;

// A platform under open sky and, below it, a sealed stone room: one surface
// spot and one cave spot in chunk (0, 0).
struct SpawnSpots {
  World world;
  glm::ivec3 surface{8, kBase + 8, 8}; // ground blocks
  glm::ivec3 cave{8, kBase, 8};

  SpawnSpots() {
    world.loadChunkSync(0, 0);
    fill(world, {2, kBase - 1, 2}, {13, kBase + 8, 13}, BlockType::STONE);
    fill(world, {4, kBase + 1, 4}, {11, kBase + 3, 11}, BlockType::AIR);
  }
  void light(glm::ivec3 at) { fill(world, at, at, BlockType::GLOWSTONE); }
};

} // namespace

TEST_CASE("the spawn rig has the light it needs") {
  SpawnSpots s;
  CHECK(s.world.getSkyLightAt(s.surface + glm::ivec3(0, 1, 0)) == 15);
  CHECK(s.world.getSkyLightAt(s.cave + glm::ivec3(0, 1, 0)) == 0);
  CHECK(s.world.getBlockLightAt(s.cave + glm::ivec3(0, 1, 0)) == 0);
}

TEST_CASE("a gloomling spawns only at night, on the surface, in the dark") {
  SpawnSpots s;
  EntityKind k = EntityKind::Gloomling;
  CHECK(enemyCanSpawn(k, s.world, s.surface, kMidnight));
  CHECK(enemyCanSpawn(k, s.world, s.surface, 0.8f));
  CHECK_FALSE(enemyCanSpawn(k, s.world, s.surface, kNoon));
  CHECK_FALSE(enemyCanSpawn(k, s.world, s.surface, 0.25f)); // sunrise
  CHECK_FALSE(enemyCanSpawn(k, s.world, s.cave, kMidnight)); // no sky

  s.light(s.surface + glm::ivec3(3, 1, 0)); // a glowstone 3 blocks away
  CHECK_FALSE(enemyCanSpawn(k, s.world, s.surface, kMidnight));
}

TEST_CASE("a glowstone far enough away doesn't stop surface spawns") {
  SpawnSpots s;
  s.light(s.surface + glm::ivec3(-5, 1, -5)); // 10 blocks of light travel
  CHECK(s.world.getBlockLightAt(s.surface + glm::ivec3(0, 1, 0)) < kEnemyMaxBlockLight);
  CHECK(enemyCanSpawn(EntityKind::Gloomling, s.world, s.surface, kMidnight));
}

TEST_CASE("a thorn crawler spawns only in its biomes") {
  const auto &biomes = entityType(EntityKind::ThornCrawler).enemy->biomes;
  World world;
  for (BiomeType b : biomes) {
    auto home = findBiomeChunk(b);
    REQUIRE(home);
    glm::ivec3 g = groundIn(world, *home, BlockType::GRASS);
    CHECK(enemyCanSpawn(EntityKind::ThornCrawler, world, g, kMidnight));
  }
  for (BiomeType b : {BiomeType::DESERT, BiomeType::OCEAN, BiomeType::TROPICAL}) {
    REQUIRE(std::find(biomes.begin(), biomes.end(), b) == biomes.end());
    auto away = findBiomeChunk(b);
    REQUIRE(away);
    glm::ivec3 g = groundIn(world, *away, BlockType::GRASS);
    CHECK_FALSE(enemyCanSpawn(EntityKind::ThornCrawler, world, g, kMidnight));
    CHECK(enemyCanSpawn(EntityKind::Gloomling, world, g, kMidnight)); // anywhere
  }
}

TEST_CASE("a zombie spawns at night on grass, a mummy at night on desert sand") {
  World world;
  auto plains = findBiomeChunk(BiomeType::PLAINS);
  auto desert = findBiomeChunk(BiomeType::DESERT);
  REQUIRE(plains);
  REQUIRE(desert);
  glm::ivec3 grass = groundIn(world, *plains, BlockType::GRASS);
  glm::ivec3 sand = groundIn(world, *desert, BlockType::SAND);

  CHECK(enemyCanSpawn(EntityKind::Zombie, world, grass, kMidnight));
  CHECK_FALSE(enemyCanSpawn(EntityKind::Zombie, world, grass, kNoon));
  CHECK_FALSE(enemyCanSpawn(EntityKind::Zombie, world, sand, kMidnight));

  CHECK(enemyCanSpawn(EntityKind::Mummy, world, sand, kMidnight));
  CHECK_FALSE(enemyCanSpawn(EntityKind::Mummy, world, sand, kNoon));
  CHECK_FALSE(enemyCanSpawn(EntityKind::Mummy, world, grass, kMidnight));
}

TEST_CASE("a cave grumble spawns only underground in the dark, at any time") {
  SpawnSpots s;
  EntityKind k = EntityKind::CaveGrumble;
  CHECK(enemyCanSpawn(k, s.world, s.cave, kMidnight));
  CHECK(enemyCanSpawn(k, s.world, s.cave, kNoon));
  CHECK_FALSE(enemyCanSpawn(k, s.world, s.surface, kMidnight));

  s.light(s.cave + glm::ivec3(3, 1, 3)); // a lit mine
  CHECK_FALSE(enemyCanSpawn(k, s.world, s.cave, kMidnight));
}

TEST_CASE("a glow crystal keeps cave grumbles away for about six blocks") {
  glm::ivec3 feet{8, kBase + 1, 8};
  {
    SpawnSpots s;
    fill(s.world, feet + glm::ivec3(3, 0, 2), feet + glm::ivec3(3, 0, 2),
         BlockType::GLOW_CRYSTAL); // 5 blocks of travel
    CHECK(s.world.getBlockLightAt(feet) == 5);
    CHECK_FALSE(enemyCanSpawn(EntityKind::CaveGrumble, s.world, s.cave, kMidnight));
  }
  {
    SpawnSpots s;
    fill(s.world, {4, kBase + 1, 4}, {4, kBase + 1, 4}, BlockType::GLOW_CRYSTAL); // 8 blocks
    CHECK(s.world.getBlockLightAt(feet) == 2);
    CHECK(enemyCanSpawn(EntityKind::CaveGrumble, s.world, s.cave, kMidnight));
  }
}

TEST_CASE("only enemies pass the enemy predicate") {
  SpawnSpots s;
  CHECK_FALSE(enemyCanSpawn(EntityKind::Woolly, s.world, s.surface, kMidnight));
  CHECK_FALSE(enemyCanSpawn(EntityKind::TestCreature, s.world, s.cave, kMidnight));
}

TEST_CASE("cave placement finds pockets below the surface, never the top") {
  SpawnSpots s;
  Rng rng{8};
  bool found_room = false;
  for (int i = 0; i < 50; ++i) {
    auto g = Spawner::caveAt(s.world, 8, 8, rng);
    REQUIRE(g);
    CHECK(g->y < s.surface.y);
    CHECK(s.world.isSolidBlock(g->x, g->y, g->z));
    CHECK(s.world.getBlockAt(glm::vec3(*g) + glm::vec3(0.5f, 1.5f, 0.5f)) == BlockType::AIR);
    CHECK(s.world.getBlockAt(glm::vec3(*g) + glm::vec3(0.5f, 2.5f, 0.5f)) == BlockType::AIR);
    found_room |= *g == s.cave;
  }
  CHECK(found_room);
  CHECK_FALSE(Spawner::caveAt(s.world, 300, 300, rng)); // unloaded
}

TEST_CASE("a column with no pocket has no cave spot") {
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {3, 0, 3}, {3, kBase, 3}, BlockType::STONE); // solid to the sky
  Rng rng{1};
  CHECK_FALSE(Spawner::caveAt(world, 3, 3, rng));
}

namespace {

bool always(const World &, glm::ivec3, float) { return true; }

// Natural terrain around the origin, for the spawner's distance band.
struct HostileRig {
  World world;
  EntityStore store;
  Spawner spawner;
  Rng rng{17};
  std::vector<glm::vec3> players{{8.0f, 100.0f, 8.0f}};

  HostileRig() {
    for (int cx = -4; cx <= 4; ++cx)
      for (int cz = -4; cz <= 4; ++cz) world.loadChunkSync(cx, cz);
  }
  void rule(int cap, Placement where = Placement::Surface) {
    SpawnRule r{EntityKind::Gloomling, cap, 50, always};
    r.placement = where;
    r.hostile = true;
    spawner.addRule(r);
  }
  void ticks(int n, Difficulty d) {
    for (int i = 0; i < n; ++i) spawner.tick(store, world, rng, players, kMidnight, d);
  }
};

} // namespace

TEST_CASE("hostile rules spawn nothing on peaceful") {
  HostileRig r;
  r.rule(4);
  r.ticks(400, Difficulty::Peaceful);
  CHECK(r.store.size() == 0);
}

TEST_CASE("hostile caps are halved on easy and full on normal") {
  HostileRig easy;
  easy.rule(4);
  easy.ticks(400, Difficulty::Easy);
  CHECK(easy.store.size() == 2);

  HostileRig normal;
  normal.rule(4);
  normal.ticks(400, Difficulty::Normal);
  CHECK(normal.store.size() == 4);
}

TEST_CASE("hard has half as many again, with half as much health again") {
  HostileRig hard;
  hard.rule(4);
  hard.ticks(400, Difficulty::Hard);
  CHECK(hard.store.size() == 6);
  for (const auto &[id, e] : hard.store.all()) CHECK(e.health == 9); // Gloomling: 6

  HostileRig normal;
  normal.rule(4);
  normal.ticks(400, Difficulty::Normal);
  for (const auto &[id, e] : normal.store.all()) CHECK(e.health == 6);
}

TEST_CASE("hard tries to spawn a third more often") {
  // One attempt per interval with a cap far above what can spawn.
  auto spawned = [](Difficulty d) {
    HostileRig r;
    SpawnRule rule{EntityKind::Gloomling, 100, 1000, always};
    rule.hostile = true;
    r.spawner.addRule(rule);
    r.ticks(1000 * 12 / EntityStore::kTickMs, d); // 12 s
    return r.store.size();
  };
  CHECK(spawned(Difficulty::Normal) == 12);
  // The first attempt waits a full interval; the rest every 750 ms.
  CHECK(spawned(Difficulty::Hard) == 15);
}

TEST_CASE("cave rules spawn below the surface, closer in") {
  HostileRig r;
  // Dig a tunnel 20 blocks east of the player, under the terrain.
  int top = Spawner::groundAt(r.world, 28, 8)->y;
  fill(r.world, {20, top - 12, 0}, {36, top - 11, 16}, BlockType::AIR);
  r.rule(3, Placement::Cave);
  r.ticks(20 * 60, Difficulty::Normal);
  REQUIRE(r.store.size() > 0);
  for (const auto &[id, e] : r.store.all()) {
    glm::vec3 p = e.body.position;
    int cx = static_cast<int>(std::floor(p.x)), cz = static_cast<int>(std::floor(p.z));
    // Under the column's topmost block (natural caves run under lakes too,
    // where groundAt refuses the water on top).
    int top = kChunkHeight - 1;
    while (top > 0 && r.world.getBlockAt(glm::vec3(cx + 0.5f, top + 0.5f, cz + 0.5f)) ==
                          BlockType::AIR)
      --top;
    CHECK(p.y < top);
    float d = glm::length(glm::vec2(p.x - 8.0f, p.z - 8.0f));
    CHECK(d >= Spawner::kCaveMinDistance - 1.5f);
    CHECK(d <= Spawner::kCaveMaxDistance + 1.5f);
  }
}

TEST_CASE("animal rules ignore difficulty") {
  HostileRig r;
  r.spawner.addRule({EntityKind::TestCreature, 4, 50, always});
  r.ticks(400, Difficulty::Peaceful);
  CHECK(r.store.size() == 4);
}

TEST_CASE("enemies walk slower when the hostile speed is scaled") {
  auto travelled = [](float scale) {
    ChaseRig r;
    r.players = {{40.5f, kBase + 1.0f, 8.5f}};
    r.store.spawn(EntityKind::ThornCrawler, {20.5f, kBase + 1.0f, 8.5f});
    r.id = r.store.all().begin()->first;
    for (int i = 0; i < 10; ++i) r.store.tick(r.world, r.rng, r.players, r.players);
    float start = r.e().body.position.x;
    for (int i = 0; i < 20; ++i) r.store.tick(r.world, r.rng, r.players, r.players, scale);
    return r.e().body.position.x - start;
  };
  float normal = travelled(1.0f), easy = travelled(0.8f);
  REQUIRE(normal > 1.0f);
  CHECK(easy / normal == doctest::Approx(0.8f).epsilon(0.03));
}
