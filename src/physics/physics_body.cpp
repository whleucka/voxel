#include "physics/physics_body.hpp"
#include "block/block_data.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

// ─── Water Detection ───────────────────────────────────────────────────

void PhysicsBody::updateWater(const World *world) {
  if (!world) {
    underwater = false;
    at_water_surface = false;
    return;
  }
  // Eyes decide "underwater"; feet decide "at the surface" (head above).
  underwater = isLiquid(world->getBlockAt(eyePosition()));
  BlockType feet = world->getBlockAt(position + glm::vec3(0.0f, 0.3f, 0.0f));
  at_water_surface = !underwater && isLiquid(feet);
}

// ─── Movement ──────────────────────────────────────────────────────────

void PhysicsBody::applyGravity(float dt) {
  if (swims && inWater()) return; // it holds its depth by swimming
  float gravity = kGravityForce;

  if (underwater) {
    gravity *= kWaterGravityMultiplier;
    // Apply water drag to vertical velocity
    velocity.y *= (1.0f - kWaterDrag * dt * 3.0f);
  } else if (at_water_surface) {
    // Smooth transition: reduced gravity at water surface to help exit
    gravity *= 0.4f;
  }

  velocity.y += gravity * dt;

  // Clamp to terminal velocity
  if (velocity.y < kTerminalVelocity) {
    velocity.y = kTerminalVelocity;
  }
}

void PhysicsBody::move(float dt, const World *world) {
  // A long frame or tick moves in sub-steps of at most half a block, so a fast
  // body can't pass through a floor or a wall between one check and the next
  // (the smallest creature is a quarter block). Gravity is applied per
  // sub-step, so a body that landed keeps pressing on the ground.
  constexpr float kMaxSubStep = 0.5f;
  // The fastest it can go this step, counting what gravity adds.
  const float fastest = std::max({std::abs(velocity.x), std::abs(velocity.z),
                                  std::abs(velocity.y) + std::abs(kGravityForce) * dt});
  const int n = std::max(1, static_cast<int>(std::ceil(fastest * dt / kMaxSubStep)));
  for (int i = 0; i < n; ++i) moveOnce(dt / static_cast<float>(n), world);

  // Apply horizontal friction (stop quickly when keys released)
  if (glides && !on_ground && !inWater()) return;
  float friction = underwater ? kWaterDrag : 0.85f;
  velocity.x *= friction;
  velocity.z *= friction;

  // Clamp tiny velocities to zero
  if (std::abs(velocity.x) < 0.01f) velocity.x = 0.0f;
  if (std::abs(velocity.z) < 0.01f) velocity.z = 0.0f;
}

void PhysicsBody::moveOnce(float dt, const World *world) {
  applyGravity(dt);

  // Move along each axis independently and resolve collisions.
  // This prevents getting stuck on corners.
  glm::vec3 old_pos = position;

  // X axis
  position.x += velocity.x * dt;
  if (collidesAt(world, position)) {
    bool can_step = on_ground || at_water_surface;
    if (!can_step || !tryAutoStep(world, position)) {
      position.x = old_pos.x;
      velocity.x = 0.0f;
    }
  }

  // Y axis
  float old_y = position.y;
  position.y += velocity.y * dt;
  if (collidesAt(world, position)) {
    position.y = old_y;
    if (velocity.y < 0.0f) {
      on_ground = true;
    }
    velocity.y = 0.0f;
  } else {
    on_ground = false;
  }

  // Z axis
  position.z += velocity.z * dt;
  if (collidesAt(world, position)) {
    bool can_step = on_ground || at_water_surface;
    if (!can_step || !tryAutoStep(world, position)) {
      position.z = old_pos.z;
      velocity.z = 0.0f;
    }
  }
}

void PhysicsBody::glide(float dt, const World *world) {
  constexpr float kMaxSubStep = 0.25f; // blocks per sub-step
  glm::vec3 delta = velocity * dt;
  float longest = std::max({std::abs(delta.x), std::abs(delta.y), std::abs(delta.z)});
  int n = std::max(1, static_cast<int>(std::ceil(longest / kMaxSubStep)));
  glm::vec3 sub = delta / static_cast<float>(n);
  on_ground = false;

  for (int i = 0; i < n; ++i) {
    for (int axis = 0; axis < 3; ++axis) {
      if (sub[axis] == 0.0f) continue;
      glm::vec3 next = position;
      next[axis] += sub[axis];
      // Stuck inside blocks: only rising is allowed, so it isn't a way
      // through walls.
      bool escaping = axis == 1 && sub.y > 0.0f && collidesAt(world, position);
      if (escaping || !collidesAt(world, next)) {
        position = next;
      } else {
        if (axis == 1 && sub.y < 0.0f) on_ground = true;
        sub[axis] = 0.0f;
      }
    }
  }
}

// ─── Auto-Step ─────────────────────────────────────────────────────────

bool PhysicsBody::tryAutoStep(const World *world, glm::vec3 &pos) const {
  // Step up onto whatever is in the way, if the body fits up to step_height
  // above it, landing on its top: the lowest clear height, so a half step
  // (a slab, a stair) is climbed without a hop, and the body stays on the
  // ground to take the next one.
  glm::vec3 stepped = pos;
  stepped.y += step_height;
  if (collidesAt(world, stepped)) return false;

  constexpr float kStep = 1.0f / 64.0f;
  for (float dy = kStep; dy < step_height; dy += kStep) {
    glm::vec3 test = pos;
    test.y += dy;
    if (!collidesAt(world, test)) {
      pos.y += dy;
      return true;
    }
  }
  pos.y = stepped.y;
  return true;
}

// ─── Collision Detection ───────────────────────────────────────────────

bool PhysicsBody::collidesAt(const World *world, glm::vec3 pos) const {
  if (!world) return false;

  float minX = pos.x - half_width;
  float maxX = pos.x + half_width;
  float minY = pos.y;
  float maxY = pos.y + height;
  float minZ = pos.z - half_width;
  float maxZ = pos.z + half_width;

  // Check all blocks that overlap the box
  int bx0 = static_cast<int>(std::floor(minX));
  int bx1 = static_cast<int>(std::floor(maxX));
  int by0 = static_cast<int>(std::floor(minY));
  int by1 = static_cast<int>(std::floor(maxY));
  int bz0 = static_cast<int>(std::floor(minZ));
  int bz1 = static_cast<int>(std::floor(maxZ));

  for (int bx = bx0; bx <= bx1; ++bx) {
    for (int by = by0; by <= by1; ++by) {
      for (int bz = bz0; bz <= bz1; ++bz) {
        if (by < 0 || by >= kChunkHeight) continue;
        const BlockType block = world->getBlockAt(glm::vec3(bx + 0.5f, by + 0.5f, bz + 0.5f));
        // Check the solid boxes in the cell (a slab's half, a stair's two
        // steps), not just the grid cell.
        const glm::vec3 cell(bx, by, bz);
        for (const DoorBox &box : blockBoxes(block)) {
          const glm::vec3 lo = cell + box.lo, hi = cell + box.hi;
          if (minX < hi.x && maxX > lo.x && minY < hi.y && maxY > lo.y && minZ < hi.z &&
              maxZ > lo.z)
            return true;
        }
      }
    }
  }
  return false;
}
