#pragma once

#include "physics/path.hpp"
#include <glm/glm.hpp>
#include <random>
#include <span>
#include <vector>

struct Entity;
class World;

using Rng = std::mt19937;

// What a creature wants to do this tick. The store turns it into velocity.
struct Intent {
  glm::vec2 move_dir{0.0f}; // horizontal, normalised or zero
  bool jump = false;
  float rise = 0.0f;        // swimmers: vertical, -1..1
  float yaw = 0.0f;         // degrees; 0 faces -Z
};

// What a brain can see: the world, the Game's RNG and where the living
// players are (feet positions).
struct BrainContext {
  const World &world;
  Rng &rng;
  std::span<const glm::vec3> targets;
};

// Decides a creature's movement each tick. Pure decision-making: the store
// applies the result through the entity's PhysicsBody.
class Brain {
public:
  virtual ~Brain() = default;
  virtual Intent think(const Entity &self, const BrainContext &ctx) = 0;
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
  Intent think(const Entity &self, const BrainContext &ctx) override;

private:
  WanderOptions opts;
  bool safeAhead(const Entity &self, const World &world) const;

  int timer_ms = 0;
  bool walking = false;
  glm::vec2 dir{0.0f};
  float yaw = 0.0f;
};

// Drifts through water in random 3D directions with pauses, and turns away
// from anything that isn't water so it never leaves it. Out of water it
// flops about until the store removes it.
class SwimBrain : public Brain {
public:
  Intent think(const Entity &self, const BrainContext &ctx) override;
  // Whether the whole body would still be in water after moving `dist`
  // along `dir` (normalised, x y z).
  static bool waterAhead(const Entity &self, const World &world, glm::vec3 dir, float dist);
  // Whether the body's middle is in water.
  static bool inWater(const Entity &self, const World &world);

private:
  int timer_ms = 0;
  bool swimming = false;
  glm::vec3 dir{0.0f};
  float yaw = 0.0f;
};

struct ChaseOptions {
  bool hop = false;  // bounce along while moving (Gloomlings)
  PathParams path;   // height_cells is taken from the creature's own height
};

// Hunts the nearest living player within kSenseRange along an A* path,
// re-planning every second or when the player moves away from the planned
// goal. With no one in range, or no way there, it wanders cautiously.
class ChaseBrain : public Brain {
public:
  static constexpr float kSenseRange = 24.0f;
  static constexpr int kReplanMs = 1000;
  static constexpr float kReplanMove = 2.0f;
  static constexpr float kWaypointReached = 0.3f; // horizontal, from the cell centre

  explicit ChaseBrain(ChaseOptions options = {});
  Intent think(const Entity &self, const BrainContext &ctx) override;
  bool chasing() const { return have_target; }

private:
  ChaseOptions opts;
  WanderBrain wander;
  std::vector<glm::ivec3> path;
  size_t next = 0;
  bool have_target = false;
  bool have_path = false;
  glm::vec3 planned_for{0.0f};
  int replan_ms = 0;
};
