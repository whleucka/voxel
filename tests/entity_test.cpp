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
    for (int x = 1; x < 15; ++x)
      for (int z = 1; z < 15; ++z) world.setBlockAt({x, kBase, z}, BlockType::STONE);
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
  for (int x = 1; x < 15; ++x)
    for (int z = 1; z < 15; ++z)
      if (x < 6 || x > 9 || z < 6 || z > 9) r.world.setBlockAt({x, kBase + 1, z}, BlockType::STONE);
  EntityId id = r.spawn(EntityKind::TestCreature, {8.0f, kBase + 1.0f, 8.0f});
  bool climbed = false;
  for (int i = 0; i < 20 * 60 && !climbed; ++i) {
    r.ticks(1);
    const Entity *e = r.store.find(id);
    climbed = e->body.on_ground && e->body.position.y > kBase + 1.9f;
  }
  CHECK(climbed);
}
