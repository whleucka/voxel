#include "physics/physics_body.hpp"
#include "block/block_data.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

namespace {

bool isSolidAt(const World *world, int bx, int by, int bz) {
  if (!world) return false;
  if (by < 0 || by >= kChunkHeight) return false;
  BlockType block = world->getBlockAt(glm::vec3(bx + 0.5f, by + 0.5f, bz + 0.5f));
  return block != BlockType::AIR && !isLiquid(block);
}

} // namespace

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

  // Apply horizontal friction (stop quickly when keys released)
  if (glides && !on_ground && !inWater()) return;
  float friction = underwater ? kWaterDrag : 0.85f;
  velocity.x *= friction;
  velocity.z *= friction;

  // Clamp tiny velocities to zero
  if (std::abs(velocity.x) < 0.01f) velocity.x = 0.0f;
  if (std::abs(velocity.z) < 0.01f) velocity.z = 0.0f;
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
  // Try stepping up by increments up to step_height.
  // If the body can fit at the stepped-up position, accept it.
  glm::vec3 stepped = pos;
  stepped.y += step_height;

  // Check that the stepped-up position is clear
  if (!collidesAt(world, stepped)) {
    // Find the actual ground level by stepping back down
    for (float dy = step_height; dy >= 0.0f; dy -= 0.05f) {
      glm::vec3 test = pos;
      test.y += dy;
      if (!collidesAt(world, test)) {
        pos.y += dy;
        return true;
      }
    }
  }

  return false;
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
        if (isSolidAt(world, bx, by, bz)) {
          // Block occupies [bx, bx+1] x [by, by+1] x [bz, bz+1]; check the
          // actual overlap, not just grid overlap.
          bool overlaps =
              minX < bx + 1.0f && maxX > static_cast<float>(bx) &&
              minY < by + 1.0f && maxY > static_cast<float>(by) &&
              minZ < bz + 1.0f && maxZ > static_cast<float>(bz);
          if (overlaps) return true;
        }
      }
    }
  }
  return false;
}
