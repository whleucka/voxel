// Characterization tests for player movement. They pin down how the player
// moves today, at a fixed 60 Hz step on blocks placed high in the sky, so the
// physics can be moved into PhysicsBody without changing how it feels.
// The numbers were recorded from the original Player implementation.
#include "doctest/doctest.h"
#include <cmath>
#include "player/player.hpp"
#include "test_util.hpp"
#include "world/world.hpp"

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kBase = 200; // floor height of the test rigs: clear sky

// A world with chunk (0,0) loaded and a 12x12 stone floor at y = kBase,
// spanning x, z in [2, 14).
struct Rig {
  World world;
  Rig() {
    world.loadChunkSync(0, 0);
    fill(world, {2, kBase, 2}, {13, kBase, 13}, BlockType::STONE);
  }
  void set(int x, int y, int z, BlockType b) { fill(world, {x, y, z}, {x, y, z}, b); }
};

void steps(Player &p, World &w, int n, const PlayerInput &in = {}) {
  for (int i = 0; i < n; ++i) {
    p.processKeyboard(kDt, in);
    p.update(kDt, &w);
  }
}

PlayerInput forward() {
  PlayerInput in;
  in.forward = true; // default camera faces -Z
  return in;
}

// Position and velocity must match the recorded values to within 1e-3
// (absolute: the y values are around 200).
void expect(const Player &p, glm::vec3 pos, glm::vec3 vel, bool ground) {
  constexpr float eps = 1e-3f;
  glm::vec3 x = p.getPosition(), v = p.getVelocity();
  for (int i = 0; i < 3; ++i) {
    CAPTURE(i);
    CHECK(std::abs(x[i] - pos[i]) < eps);
    CHECK(std::abs(v[i] - vel[i]) < eps);
  }
  CHECK(p.isOnGround() == ground);
}

} // namespace

TEST_CASE("falling lands on the floor") {
  Rig r;
  Player p({8.5f, kBase + 6.0f, 8.5f});
  steps(p, r.world, 30);
  expect(p, {8.5f, 202.383331f, 8.5f}, {0.0f, -14.000006f, 0.0f}, false);
  steps(p, r.world, 90);
  expect(p, {8.5f, 201.006668f, 8.5f}, {0.0f, 0.0f, 0.0f}, true);
}

TEST_CASE("walking into a wall stops at the wall") {
  Rig r;
  for (int y = kBase + 1; y <= kBase + 2; ++y)
    for (int x = 2; x < 14; ++x) r.set(x, y, 4, BlockType::STONE);
  Player p({8.5f, kBase + 1.0f, 10.5f});
  steps(p, r.world, 5);
  steps(p, r.world, 120, forward());
  expect(p, {8.5f, 201.0f, 5.319603f}, {0.0f, 0.0f, 0.0f}, true);
}

TEST_CASE("a full-block ledge is not stepped up") {
  Rig r;
  for (int x = 2; x < 14; ++x) r.set(x, kBase + 1, 4, BlockType::STONE);
  Player p({8.5f, kBase + 1.0f, 10.5f});
  steps(p, r.world, 5);
  steps(p, r.world, 120, forward());
  expect(p, {8.5f, 201.0f, 5.319603f}, {0.0f, 0.0f, 0.0f}, true);
}

TEST_CASE("jumping while walking climbs a one-block ledge") {
  Rig r;
  for (int x = 2; x < 14; ++x)
    for (int z = 2; z <= 4; ++z) r.set(x, kBase + 1, z, BlockType::STONE);
  Player p({8.5f, kBase + 1.0f, 6.5f});
  steps(p, r.world, 5);
  PlayerInput in = forward();
  in.jump = true;
  steps(p, r.world, 60, in);
  expect(p, {8.5f, 202.49556f, 2.183002f}, {0.0f, 6.133333f, -3.66945f}, false);
}

TEST_CASE("water slows a fall") {
  Rig r;
  for (int y = kBase + 1; y <= kBase + 8; ++y)
    for (int x = 6; x <= 10; ++x)
      for (int z = 6; z <= 10; ++z) r.set(x, y, z, BlockType::WATER);
  Player p({8.5f, kBase + 12.0f, 8.5f});
  steps(p, r.world, 40);
  expect(p, {8.5f, 206.099243f, 8.5f}, {0.0f, -12.271027f, 0.0f}, false);
  steps(p, r.world, 60);
  expect(p, {8.5f, 201.000809f, 8.5f}, {0.0f, 0.0f, 0.0f}, true);
}

TEST_CASE("friction stops horizontal motion") {
  Rig r;
  Player p({8.5f, kBase + 1.0f, 8.5f});
  steps(p, r.world, 5);
  steps(p, r.world, 3, forward());
  expect(p, {8.5f, 201.0f, 8.28415f}, {0.0f, 0.0f, -3.66945f}, true);
  steps(p, r.world, 30);
  expect(p, {8.5f, 201.0f, 7.879545f}, {0.0f, 0.0f, -0.028001f}, true);
}

TEST_CASE("crouching lowers the player and slows walking") {
  Rig r;
  Player p({8.5f, kBase + 1.0f, 10.5f});
  steps(p, r.world, 5);
  PlayerInput in = forward();
  in.crouch = true;
  steps(p, r.world, 30, in);
  expect(p, {8.5f, 201.0f, 9.852493f}, {0.0f, 0.0f, -1.10075f}, true);
  CHECK(p.getEyePosition().y == doctest::Approx(202.27f));
}

TEST_CASE("a knock pushes the player, and input waits a moment to steer") {
  Rig r;
  Player p({8.5f, kBase + 1.0f, 8.5f});
  steps(p, r.world, 30); // settle on the floor
  REQUIRE(p.isOnGround());

  // Knocked toward +Z while walking forward (-Z).
  p.knock({0.0f, 5.0f, 7.0f});
  CHECK(p.getVelocity().y == doctest::Approx(5.0f));
  steps(p, r.world, 3, forward());
  CHECK(p.getVelocity().z > 0.0f); // still flying back, not steered
  CHECK(p.getPosition().z > 8.5f);
  CHECK(p.getPosition().y > kBase + 1.0f);

  steps(p, r.world, 30, forward()); // well past the steer delay
  CHECK(p.getVelocity().z < 0.0f);
}

TEST_CASE("fly mode ignores knocks") {
  Rig r;
  Player p({8.5f, kBase + 5.0f, 8.5f});
  p.toggleFlyMode();
  p.knock({0.0f, 5.0f, 7.0f});
  CHECK(p.getVelocity() == glm::vec3(0.0f));
}

TEST_CASE("flying stops at a wall") {
  Rig r;
  fill(r.world, {2, kBase + 1, 4}, {13, kBase + 4, 4}, BlockType::STONE); // wall at z = 4
  Player p({8.5f, kBase + 1.0f, 8.5f});
  p.toggleFlyMode();
  PlayerInput in = forward();
  in.sprint = true;
  steps(p, r.world, 60, in);
  CHECK(p.getPosition().z >= 5.0f + kPlayerHalfWidth - 1e-3f);
  CHECK(p.getPosition().z < 5.0f + kPlayerHalfWidth + 0.3f);
}

TEST_CASE("flying fast doesn't skip through a thin wall") {
  Rig r;
  fill(r.world, {2, kBase + 1, 4}, {13, kBase + 4, 4}, BlockType::STONE);
  Player p({8.5f, kBase + 1.0f, 8.5f});
  p.toggleFlyMode();
  PlayerInput in = forward();
  in.sprint = true;
  steps(p, r.world, 1, in);
  for (int i = 0; i < 4; ++i) {
    p.processKeyboard(0.5f, in); // a long frame: 14 blocks in one step
    p.update(0.5f, &r.world);
  }
  CHECK(p.getPosition().z > 4.0f);
}

TEST_CASE("flying down stops on the floor") {
  Rig r;
  Player p({8.5f, kBase + 4.0f, 8.5f});
  p.toggleFlyMode();
  PlayerInput in;
  in.fly_down = true;
  steps(p, r.world, 120, in);
  CHECK(p.getPosition().y >= kBase + 1.0f - 1e-3f);
  CHECK(p.getPosition().y < kBase + 1.3f);
  CHECK(p.getVelocity() == glm::vec3(0.0f));
}

TEST_CASE("flying slides along a wall") {
  Rig r;
  fill(r.world, {2, kBase + 1, 4}, {13, kBase + 4, 4}, BlockType::STONE);
  Player p({8.5f, kBase + 1.0f, 5.5f});
  p.toggleFlyMode();
  PlayerInput in = forward();
  in.right = true; // into the wall and along it (+X)
  steps(p, r.world, 20, in);
  CHECK(p.getPosition().z >= 5.0f + kPlayerHalfWidth - 1e-3f);
  CHECK(p.getPosition().x > 9.5f);
}

TEST_CASE("flying out of a block you're stuck in") {
  Rig r;
  Player p({8.5f, kBase + 0.5f, 8.5f}); // feet inside the floor
  p.toggleFlyMode();
  PlayerInput in;
  in.fly_up = true;
  steps(p, r.world, 30, in);
  CHECK(p.getPosition().y > kBase + 2.0f);
}

TEST_CASE("stuck in a block, flying sideways doesn't pass through") {
  Rig r;
  fill(r.world, {2, kBase + 1, 2}, {13, kBase + 3, 13}, BlockType::STONE); // solid rock
  Player p({8.5f, kBase + 1.0f, 8.5f});
  p.toggleFlyMode();
  steps(p, r.world, 30, forward());
  CHECK(p.getPosition().z == doctest::Approx(8.5f));
}

TEST_CASE("flying is only possible where the world allows it") {
  Player p({8.5f, kBase + 5.0f, 8.5f});
  p.toggleFlyMode();
  REQUIRE(p.isFlyMode());
  p.setFlyAllowed(false); // told the world is survival
  CHECK_FALSE(p.isFlyMode());
  p.toggleFlyMode();      // T
  CHECK_FALSE(p.isFlyMode());
  p.setFlyAllowed(true);
  p.toggleFlyMode();
  CHECK(p.isFlyMode());
}
