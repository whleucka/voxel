#pragma once

#include "core/constants.hpp"
#include <glm/glm.hpp>

class World;

// An axis-aligned box that falls, swims and collides with blocks. Shared by
// the player and by entities so they all move by the same rules. Position is
// the centre of the box's base (the feet).
struct PhysicsBody {
  glm::vec3 position{0.0f};
  glm::vec3 velocity{0.0f};
  float half_width = kPlayerHalfWidth;
  float height = kPlayerHeight;
  float eye_height = kPlayerEyeHeight;
  float step_height = kPlayerStepHeight;

  bool swims = false;  // fish: no gravity in water; brains steer velocity.y
  bool glides = false; // thrown things: no horizontal friction in the air

  bool on_ground = false;
  bool underwater = false;       // eyes in liquid
  bool at_water_surface = false; // feet in liquid, eyes above it

  glm::vec3 eyePosition() const { return position + glm::vec3(0.0f, eye_height, 0.0f); }
  bool inWater() const { return underwater || at_water_surface; }

  // Refresh underwater / at_water_surface from the blocks at eyes and feet.
  void updateWater(const World *world);
  // One physics step: gravity and buoyancy, per-axis movement with collision
  // and auto-step, then horizontal friction. Call updateWater() first.
  void move(float dt, const World *world);
  // Fly-mode step: moves by velocity with no gravity or friction, stopping at
  // blocks one axis at a time (so it slides along walls). Small sub-steps keep
  // fast flight from passing through thin walls. A body already stuck inside
  // blocks can still rise, so it can fly out the top.
  void glide(float dt, const World *world);
  // Both of the above.
  void step(float dt, const World *world) {
    updateWater(world);
    move(dt, world);
  }

  bool collidesAt(const World *world, glm::vec3 pos) const;

private:
  void applyGravity(float dt);
  // One sub-step of move(): gravity, then each axis with collision.
  void moveOnce(float dt, const World *world);
  bool tryAutoStep(const World *world, glm::vec3 &pos) const;
};
