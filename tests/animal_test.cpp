#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "entity/entity_store.hpp"
#include "test_util.hpp"
#include "world/world.hpp"

namespace {

constexpr int kBase = 200; // clear sky above chunk (0,0)

// An entity with a chosen brain on a hand-built rig in the sky.
struct BrainRig {
  World world;
  EntityStore store;
  Rng rng{77};
  std::vector<glm::vec3> players{{8.0f, kBase + 1.0f, 8.0f}};
  EntityId id = 0;

  BrainRig() { world.loadChunkSync(0, 0); }

  void spawn(glm::vec3 at, WanderOptions opts) {
    auto e = store.spawn(EntityKind::TestCreature, at);
    REQUIRE(e);
    id = *e;
    store.find(id)->brain = std::make_unique<WanderBrain>(opts);
  }
  const Entity &e() const { return *store.find(id); }
  void tick() { store.tick(world, rng, players); }
};

WanderOptions cautious(bool swim = false) {
  WanderOptions o;
  o.cautious = true;
  o.can_swim = swim;
  return o;
}

} // namespace

TEST_CASE("a cautious walker stays on an island ringed by water") {
  BrainRig r;
  fill(r.world, {2, kBase, 2}, {13, kBase, 13}, BlockType::WATER);
  fill(r.world, {6, kBase, 6}, {9, kBase, 9}, BlockType::STONE);
  r.spawn({8.0f, kBase + 1.0f, 8.0f}, cautious());
  for (int i = 0; i < 20 * 40; ++i) {
    r.tick();
    glm::vec3 p = r.e().body.position;
    REQUIRE(p.x > 5.4f);
    REQUIRE(p.x < 10.6f);
    REQUIRE(p.z > 5.4f);
    REQUIRE(p.z < 10.6f);
    REQUIRE_FALSE(r.e().body.inWater());
  }
}

TEST_CASE("a cautious walker won't step off a high platform") {
  BrainRig r;
  fill(r.world, {6, kBase, 6}, {9, kBase, 9}, BlockType::STONE); // nothing below
  r.spawn({8.0f, kBase + 1.0f, 8.0f}, cautious());
  for (int i = 0; i < 20 * 40; ++i) {
    r.tick();
    REQUIRE(r.e().body.position.y > kBase + 0.5f);
  }
}

TEST_CASE("a cautious walker steps down a single block") {
  BrainRig r;
  fill(r.world, {1, kBase, 1}, {14, kBase, 14}, BlockType::STONE);
  fill(r.world, {7, kBase + 1, 7}, {8, kBase + 1, 8}, BlockType::STONE);
  r.spawn({8.0f, kBase + 2.0f, 8.0f}, cautious());
  bool stepped_down = false;
  for (int i = 0; i < 20 * 60 && !stepped_down; ++i) {
    r.tick();
    stepped_down = r.e().body.on_ground && r.e().body.position.y < kBase + 1.5f;
  }
  CHECK(stepped_down);
}

TEST_CASE("a swimmer may walk into water") {
  BrainRig r;
  fill(r.world, {2, kBase, 2}, {13, kBase, 13}, BlockType::STONE);
  fill(r.world, {2, kBase + 1, 2}, {13, kBase + 1, 5}, BlockType::WATER);
  fill(r.world, {2, kBase + 1, 10}, {13, kBase + 1, 13}, BlockType::WATER);
  fill(r.world, {2, kBase + 1, 2}, {4, kBase + 1, 13}, BlockType::WATER);
  fill(r.world, {11, kBase + 1, 2}, {13, kBase + 1, 13}, BlockType::WATER);
  r.spawn({8.0f, kBase + 1.0f, 8.0f}, cautious(/*swim=*/true));
  bool swam = false;
  for (int i = 0; i < 20 * 60 && !swam; ++i) {
    r.tick();
    swam = r.e().body.inWater();
  }
  CHECK(swam);
}

TEST_CASE("a hopper jumps while walking on flat ground") {
  BrainRig r;
  fill(r.world, {1, kBase, 1}, {14, kBase, 14}, BlockType::STONE);
  WanderOptions o;
  o.hop = true;
  r.spawn({8.0f, kBase + 1.0f, 8.0f}, o);
  bool hopped = false;
  for (int i = 0; i < 20 * 20 && !hopped; ++i) {
    r.tick();
    hopped = r.e().body.position.y > kBase + 1.3f;
  }
  CHECK(hopped);
}

// ─── Animal definitions and spawning ───────────────────────────────────

#include "biome/biome_manager.hpp"
#include "entity/animals.hpp"

namespace {

// The first chunk (spiralling out from the origin) whose biome is `b`.
std::optional<glm::ivec2> findBiomeChunk(BiomeType b) {
  for (int r = 0; r < 300; ++r)
    for (int dx = -r; dx <= r; ++dx)
      for (int dz = -r; dz <= r; ++dz) {
        if (std::abs(dx) != r && std::abs(dz) != r) continue;
        if (BiomeManager::getBiomeForChunk(dx, dz) == b) return glm::ivec2(dx, dz);
      }
  return std::nullopt;
}

// A ground cell in chunk c, high in the sky, set to the given block.
glm::ivec3 groundIn(World &world, glm::ivec2 c, BlockType b) {
  world.loadChunkSync(c.x, c.y);
  glm::ivec3 g{c.x * kChunkWidth + 5, 180, c.y * kChunkDepth + 5};
  fill(world, g, g, b);
  return g;
}

} // namespace

TEST_CASE("the four animals are friendly, sized and fed") {
  for (EntityKind k : kAnimalKinds) {
    const EntityTypeDef &d = entityType(k);
    CAPTURE(d.name);
    REQUIRE(isAnimal(k));
    CHECK(d.max_health == 0); // can't be hit
    CHECK(d.walk_speed > 0.0f);
    CHECK(d.brain);
    CHECK(itemDef(d.animal->favourite).places.has_value());
    CHECK(isValidItem(d.animal->product.id));
    CHECK_FALSE(d.animal->biomes.empty());
    CHECK_FALSE(d.animal->ground.empty());
    int swinging = 0;
    for (const ModelBox &b : d.model) swinging += b.swing != 0;
    CHECK(swinging == 4);
  }
  CHECK_FALSE(isAnimal(EntityKind::TestCreature));
  CHECK_FALSE(isAnimal(EntityKind::DroppedItem));
  CHECK(entityType(EntityKind::Bunny).animal->product.id == ItemId::BERRIES);
  CHECK(entityType(EntityKind::BlossomDeer).animal->product.id == ItemId::SWEET_PETALS);
}

TEST_CASE("each animal spawns only in its biome, on its ground, by day") {
  for (EntityKind k : kAnimalKinds) {
    const AnimalDef &a = *entityType(k).animal;
    CAPTURE(entityType(k).name);
    World world;
    auto home = findBiomeChunk(a.biomes[0]);
    REQUIRE(home);
    glm::ivec3 good = groundIn(world, *home, a.ground[0]);

    CHECK(animalCanSpawn(k, world, good, 0.5f));            // noon
    CHECK_FALSE(animalCanSpawn(k, world, good, 0.0f));      // midnight
    CHECK_FALSE(animalCanSpawn(k, world, good, 0.9f));      // late evening

    glm::ivec3 rock = good + glm::ivec3(2, 0, 0);
    fill(world, rock, rock, BlockType::BEDROCK);            // nobody's ground
    CHECK_FALSE(animalCanSpawn(k, world, rock, 0.5f));

    // A biome it doesn't live in.
    BiomeType elsewhere = BiomeType::DESERT;
    for (BiomeType b : {BiomeType::DESERT, BiomeType::OCEAN, BiomeType::MOUNTAIN})
      if (std::find(a.biomes.begin(), a.biomes.end(), b) == a.biomes.end()) {
        elsewhere = b;
        break;
      }
    auto away = findBiomeChunk(elsewhere);
    REQUIRE(away);
    glm::ivec3 wrong = groundIn(world, *away, a.ground[0]);
    CHECK_FALSE(animalCanSpawn(k, world, wrong, 0.5f));
  }
}

TEST_CASE("on generated terrain, animals appear only where they live") {
  World world;
  for (int cx = -5; cx <= 5; ++cx)
    for (int cz = -5; cz <= 5; ++cz) world.loadChunkSync(cx, cz);
  EntityStore store;
  Spawner spawner;
  registerAnimalSpawns(spawner);
  Rng rng{2024};
  std::vector<glm::vec3> players{{8.0f, 100.0f, 8.0f}};
  for (int i = 0; i < 20 * 60; ++i) spawner.tick(store, world, rng, players, 0.5f);

  REQUIRE(store.size() > 0); // something lives around the origin
  for (const auto &[id, e] : store.all()) {
    REQUIRE(isAnimal(e.kind));
    const AnimalDef &a = *entityType(e.kind).animal;
    int cx = static_cast<int>(std::floor(e.body.position.x / kChunkWidth));
    int cz = static_cast<int>(std::floor(e.body.position.z / kChunkDepth));
    BiomeType b = BiomeManager::getBiomeForChunk(cx, cz);
    CAPTURE(entityType(e.kind).name);
    CHECK(std::find(a.biomes.begin(), a.biomes.end(), b) != a.biomes.end());
  }
}

TEST_CASE("no animals spawn at night") {
  World world;
  for (int cx = -5; cx <= 5; ++cx)
    for (int cz = -5; cz <= 5; ++cz) world.loadChunkSync(cx, cz);
  EntityStore store;
  Spawner spawner;
  registerAnimalSpawns(spawner);
  Rng rng{2024};
  std::vector<glm::vec3> players{{8.0f, 100.0f, 8.0f}};
  for (int i = 0; i < 20 * 30; ++i) spawner.tick(store, world, rng, players, 0.0f);
  CHECK(store.size() == 0);
}
