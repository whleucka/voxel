#include "doctest/doctest.h"
#include "entity/entity_store.hpp"
#include "test_util.hpp"
#include "world/world.hpp"

namespace {

constexpr int kBase = 200; // floor height of the rig: clear sky

// Chunk (0,0) loaded with a stone floor at y = kBase over x, z in [1, 15).
struct Rig {
  World world;
  EntityStore store;
  Rng rng{1234};
  std::vector<glm::vec3> players{{8.0f, kBase + 1.0f, 8.0f}};

  Rig() {
    world.loadChunkSync(0, 0);
    fill(world, {1, kBase, 1}, {14, kBase, 14}, BlockType::STONE);
  }

  void ticks(int n) {
    for (int i = 0; i < n; ++i) store.tick(world, rng, players);
  }

  EntityId spawn(EntityKind k, glm::vec3 p) {
    auto id = store.spawn(k, p);
    REQUIRE(id);
    return *id;
  }
};

} // namespace

TEST_CASE("entity kinds are defined") {
  CHECK(isValidEntityKind(EntityKind::DroppedItem));
  CHECK(isValidEntityKind(EntityKind::TestCreature));
  CHECK_FALSE(isValidEntityKind(static_cast<EntityKind>(999)));
  CHECK(entityType(EntityKind::DroppedItem).max_health == 0);
  CHECK(entityType(EntityKind::TestCreature).max_health > 0);
  CHECK_FALSE(entityType(EntityKind::TestCreature).model.empty());
}

TEST_CASE("a spawned entity falls and rests on the ground") {
  Rig r;
  EntityId id = r.spawn(EntityKind::DroppedItem, {8.5f, kBase + 5.0f, 8.5f});
  r.ticks(60);
  const Entity *e = r.store.find(id);
  REQUIRE(e);
  CHECK(e->body.on_ground);
  CHECK(e->body.position.y == doctest::Approx(kBase + 1.0f).epsilon(0.001));
  CHECK(e->body.position.x == 8.5f);
}

TEST_CASE("entities in unloaded chunks freeze") {
  Rig r;
  r.players = {{5 * 16.0f, 100.0f, 5 * 16.0f}};
  glm::vec3 start{5 * 16.0f + 8, 150.0f, 5 * 16.0f + 8};
  EntityId id = r.spawn(EntityKind::DroppedItem, start);
  r.ticks(40);
  CHECK(r.store.find(id)->body.position == start);
}

TEST_CASE("entities far from every player despawn") {
  Rig r;
  EntityId near = r.spawn(EntityKind::DroppedItem, {8.5f, kBase + 1.0f, 8.5f});
  EntityId far = r.spawn(EntityKind::DroppedItem, {8.5f, kBase + 1.0f, 8.5f + 120.0f});
  r.ticks(1);
  CHECK(r.store.find(near));
  CHECK_FALSE(r.store.find(far));
}

TEST_CASE("with no players nothing despawns by distance") {
  Rig r;
  r.players.clear();
  EntityId id = r.spawn(EntityKind::DroppedItem, {8.5f, kBase + 1.0f, 500.0f});
  r.ticks(5);
  CHECK(r.store.find(id));
}

TEST_CASE("dropped items despawn after 5 minutes") {
  Rig r;
  EntityId id = r.spawn(EntityKind::DroppedItem, {8.5f, kBase + 1.0f, 8.5f});
  r.ticks(5 * 60 * 20 - 1);
  CHECK(r.store.find(id));
  r.ticks(1);
  CHECK_FALSE(r.store.find(id));
}

TEST_CASE("creatures don't despawn by age") {
  Rig r;
  EntityId id = r.spawn(EntityKind::TestCreature, {8.5f, kBase + 1.0f, 8.5f});
  r.ticks(6 * 60 * 20);
  CHECK(r.store.find(id));
}

TEST_CASE("the entity cap") {
  Rig r;
  for (size_t i = 0; i < EntityStore::kMaxEntities; ++i)
    REQUIRE(r.store.spawn(EntityKind::DroppedItem, {8.5f, kBase + 1.0f, 8.5f}));
  CHECK_FALSE(r.store.spawn(EntityKind::DroppedItem, {8.5f, kBase + 1.0f, 8.5f}));
  CHECK_FALSE(r.store.spawn(static_cast<EntityKind>(999), {0, 0, 0}));
}

TEST_CASE("spawned creatures start at full health with a brain") {
  Rig r;
  EntityId id = r.spawn(EntityKind::TestCreature, {8.5f, kBase + 1.0f, 8.5f});
  const Entity *e = r.store.find(id);
  CHECK(e->health == entityType(EntityKind::TestCreature).max_health);
  CHECK(e->brain);
  CHECK(e->body.half_width == entityType(EntityKind::TestCreature).half_width);
}

TEST_CASE("the test creature wanders") {
  Rig r;
  glm::vec3 start{8.5f, kBase + 1.0f, 8.5f};
  EntityId id = r.spawn(EntityKind::TestCreature, start);
  float furthest = 0.0f;
  for (int i = 0; i < 20 * 30; ++i) {
    r.ticks(1);
    glm::vec3 p = r.store.find(id)->body.position;
    furthest = std::max(furthest, glm::distance(glm::vec2(p.x, p.z), glm::vec2(start.x, start.z)));
  }
  CHECK(furthest > 1.5f);
}

TEST_CASE("a wandering creature hops up a one-block step") {
  // Raise everything but the centre 4x4: wherever it walks, it meets a step.
  Rig r;
  fill(r.world, {1, kBase + 1, 1}, {14, kBase + 1, 14}, BlockType::STONE);
  fill(r.world, {6, kBase + 1, 6}, {9, kBase + 1, 9}, BlockType::AIR);
  EntityId id = r.spawn(EntityKind::TestCreature, {8.0f, kBase + 1.0f, 8.0f});
  bool climbed = false;
  for (int i = 0; i < 20 * 60 && !climbed; ++i) {
    r.ticks(1);
    const Entity *e = r.store.find(id);
    climbed = e->body.on_ground && e->body.position.y > kBase + 1.9f;
  }
  CHECK(climbed);
}

// ─── Spawner ───────────────────────────────────────────────────────────

#include "block/block_data.hpp"
#include "entity/spawner.hpp"

namespace {

// Natural terrain loaded 4 chunks around the origin: enough for the whole
// 24..64 block spawn band around a player standing at the centre.
struct SpawnRig {
  World world;
  EntityStore store;
  Spawner spawner;
  Rng rng{99};
  std::vector<glm::vec3> players{{8.0f, 100.0f, 8.0f}};

  explicit SpawnRig(int radius = 4) {
    for (int cx = -radius; cx <= radius; ++cx)
      for (int cz = -radius; cz <= radius; ++cz) world.loadChunkSync(cx, cz);
  }
  void ticks(int n) {
    for (int i = 0; i < n; ++i) spawner.tick(store, world, rng, players, 0.5f);
  }
  int count(EntityKind k) const {
    int n = 0;
    for (const auto &[id, e] : store.all()) n += e.kind == k;
    return n;
  }
};

bool always(const World &, glm::ivec3, float) { return true; }
bool never(const World &, glm::ivec3, float) { return false; }

} // namespace

TEST_CASE("spawn rules fill up to their per-player cap") {
  SpawnRig r;
  r.spawner.addRule({EntityKind::TestCreature, 3, 100, always});
  r.ticks(20 * 20);
  CHECK(r.count(EntityKind::TestCreature) == 3);
}

TEST_CASE("spawns land 24..64 blocks away, standing on loaded ground") {
  SpawnRig r;
  r.spawner.addRule({EntityKind::TestCreature, 10, 50, always});
  r.ticks(20 * 20);
  REQUIRE(r.count(EntityKind::TestCreature) > 0);
  for (const auto &[id, e] : r.store.all()) {
    glm::vec2 d(e.body.position.x - 8.0f, e.body.position.z - 8.0f);
    CHECK(glm::length(d) >= Spawner::kMinDistance - 1.5f);
    CHECK(glm::length(d) <= Spawner::kMaxDistance + 1.5f);
    // Standing on solid ground with air at feet and head.
    glm::vec3 p = e.body.position;
    BlockType below = r.world.getBlockAt(p - glm::vec3(0, 0.5f, 0));
    CHECK(below != BlockType::AIR);
    CHECK_FALSE(isLiquid(below));
    CHECK(r.world.getBlockAt(p + glm::vec3(0, 0.5f, 0)) == BlockType::AIR);
  }
}

TEST_CASE("a rule whose predicate refuses never spawns") {
  SpawnRig r(1);
  r.spawner.addRule({EntityKind::TestCreature, 5, 50, never});
  r.ticks(200);
  CHECK(r.store.size() == 0);
}

TEST_CASE("nothing spawns into unloaded chunks") {
  World world;
  world.loadChunkSync(0, 0); // only the player's own chunk: the band is beyond it
  EntityStore store;
  Spawner spawner;
  Rng rng{5};
  spawner.addRule({EntityKind::TestCreature, 5, 50, always});
  std::vector<glm::vec3> players{{8.0f, 100.0f, 8.0f}};
  for (int i = 0; i < 400; ++i) spawner.tick(store, world, rng, players, 0.5f);
  CHECK(store.size() == 0);
}

TEST_CASE("groundAt finds standable surface and skips water") {
  World world;
  world.loadChunkSync(0, 0);
  for (int y = 150; y < kChunkHeight; ++y) world.setBlockAt({3, y, 3}, BlockType::AIR);
  world.setBlockAt({3, 150, 3}, BlockType::STONE);
  auto g = Spawner::groundAt(world, 3, 3);
  REQUIRE(g);
  CHECK(*g == glm::ivec3(3, 150, 3));

  world.setBlockAt({3, 151, 3}, BlockType::WATER);
  CHECK_FALSE(Spawner::groundAt(world, 3, 3));
  CHECK_FALSE(Spawner::groundAt(world, 300, 300)); // unloaded
}
