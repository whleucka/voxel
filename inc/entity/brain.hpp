#pragma once

#include <glm/glm.hpp>
#include <random>

struct Entity;
class World;

using Rng = std::mt19937;

// What a creature wants to do this tick. The store turns it into velocity.
struct Intent {
  glm::vec2 move_dir{0.0f}; // horizontal, normalised or zero
  bool jump = false;
  float yaw = 0.0f;         // degrees; 0 faces -Z
};

// Decides a creature's movement each tick. Pure decision-making: the store
// applies the result through the entity's PhysicsBody.
class Brain {
public:
  virtual ~Brain() = default;
  virtual Intent think(const Entity &self, const World &world, Rng &rng) = 0;
};

struct WanderOptions {
  // Look before stepping: turn around rather than walk into water (unless
  // can_swim) or off a drop of more than kMaxSafeDrop blocks.
  bool cautious = false;
  bool can_swim = false;
  bool hop = false; // jump whenever on the ground while walking (bunnies)
};

// Walks in a random direction for a while, stands still for a while, and
// jumps when something is in the way.
class WanderBrain : public Brain {
public:
  static constexpr int kMaxSafeDrop = 2;

  explicit WanderBrain(WanderOptions options = {}) : opts(options) {}
  Intent think(const Entity &self, const World &world, Rng &rng) override;

private:
  WanderOptions opts;
  bool safeAhead(const Entity &self, const World &world) const;

  int timer_ms = 0;
  bool walking = false;
  glm::vec2 dir{0.0f};
  float yaw = 0.0f;
};
