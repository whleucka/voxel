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
