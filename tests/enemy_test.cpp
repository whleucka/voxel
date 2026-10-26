#include "doctest/doctest.h"
#include "block/block_type.hpp"
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
