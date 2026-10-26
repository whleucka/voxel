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

// Walks in a random direction for a while, stands still for a while, and
// jumps when something is in the way.
class WanderBrain : public Brain {
public:
  Intent think(const Entity &self, const World &world, Rng &rng) override;

private:
  int timer_ms = 0;
  bool walking = false;
  glm::vec2 dir{0.0f};
  float yaw = 0.0f;
};
