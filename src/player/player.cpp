#include "player/player.hpp"
#include "block/block_data.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

Player::Player(glm::vec3 start_pos)
    : position(start_pos), velocity(0.0f) {
  camera.position = getEyePosition();

  // Pre-set hotbar slots with static blocks for testing
  hotbar_blocks[0] = BlockType::DIRT;
  hotbar_blocks[1] = BlockType::GRASS;
  hotbar_blocks[2] = BlockType::STONE;
  hotbar_blocks[3] = BlockType::COBBLESTONE;
  hotbar_blocks[4] = BlockType::OAK_LOG;
  hotbar_blocks[5] = BlockType::OAK_LEAF;
  hotbar_blocks[6] = BlockType::SAND;
  hotbar_blocks[7] = BlockType::SNOW;
  hotbar_blocks[8] = BlockType::DIAMOND_ORE;
}

glm::vec3 Player::getEyePosition() const {
  return position + glm::vec3(0.0f, getPlayerEyeHeight(), 0.0f);
}

float Player::getPlayerHeight() const {
  return crouching ? kPlayerCrouchHeight : kPlayerHeight;
}

float Player::getPlayerEyeHeight() const {
  return crouching ? kPlayerCrouchEyeHeight : kPlayerEyeHeight;
}

void Player::setSelectedHotbarSlot(int slot) {
  if (slot >= 0 && slot < kHotbarSlots) {
    selected_hotbar_slot = slot;
  }
}

BlockType Player::getHotbarBlock() const {
  return hotbar_blocks[selected_hotbar_slot];
}

BlockType Player::getHotbarBlock(int slot) const {
  if (slot >= 0 && slot < kHotbarSlots) {
    return hotbar_blocks[slot];
  }
  return BlockType::AIR;
}

// ─── Update ────────────────────────────────────────────────────────────

void Player::update(float dt, World* world) {
  checkWater(world);

  if (!fly_mode) {
    applyGravity(dt);

    // Apply velocity to position with per-axis collision resolution
    glm::vec3 old_pos = position;

    // Move along each axis independently and resolve collisions.
    // This prevents getting stuck on corners.

    // X axis
    position.x += velocity.x * dt;
    if (collidesWithWorld(world, position)) {
      bool can_step = on_ground || at_water_surface;
      if (!can_step || !tryAutoStep(world, position, old_pos.x, 0)) {
        position.x = old_pos.x;
        velocity.x = 0.0f;
      }
    }

    // Y axis
    float old_y = position.y;
    position.y += velocity.y * dt;
    if (collidesWithWorld(world, position)) {
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
    if (collidesWithWorld(world, position)) {
      bool can_step = on_ground || at_water_surface;
      if (!can_step || !tryAutoStep(world, position, old_pos.z, 2)) {
        position.z = old_pos.z;
        velocity.z = 0.0f;
      }
    }

    // Apply horizontal friction (stop quickly when keys released)
    float friction = underwater ? kWaterDrag : 0.85f;
    velocity.x *= friction;
    velocity.z *= friction;

    // Clamp tiny velocities to zero
    if (std::abs(velocity.x) < 0.01f) velocity.x = 0.0f;
    if (std::abs(velocity.z) < 0.01f) velocity.z = 0.0f;
  }

  camera.position = getEyePosition();
}

// ─── Gravity ───────────────────────────────────────────────────────────

void Player::applyGravity(float dt) {
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

// ─── Keyboard Processing ───────────────────────────────────────────────

void Player::processKeyboard(float dt, const PlayerInput& input) {
  if (fly_mode) {
    // Fly mode: full 3D movement along camera direction
    glm::vec3 direction = glm::normalize(camera.front);
    float speed = input.sprint ? kPlayerSprintSpeed * 5.0f : kPlayerMoveSpeed * 3.0f;

    if (input.forward)  position += direction * speed * dt;
    if (input.backward) position -= direction * speed * dt;
    if (input.left)     position -= camera.right * speed * dt;
    if (input.right)    position += camera.right * speed * dt;
    if (input.fly_up)   position.y += speed * dt;
    if (input.fly_down) position.y -= speed * dt;

    velocity = glm::vec3(0.0f); // No physics in fly mode
    return;
  }

  // ── Walk / Sprint / Crouch mode ──

  // Crouch state
  crouching = input.crouch;

  // Sprint: only while moving forward, not crouching, and not underwater
  sprinting = input.sprint && input.forward && !crouching;

  // Determine movement speed
  float speed;
  if (underwater) {
    speed = sprinting ? kPlayerWaterSprintSpeed : kPlayerWaterSpeed;
  } else if (crouching) {
    speed = kPlayerCrouchSpeed;
  } else if (sprinting) {
    speed = kPlayerSprintSpeed;
  } else {
    speed = kPlayerMoveSpeed;
  }

  // Direction on XZ plane (no vertical component for walking)
  glm::vec3 flat_front = glm::vec3(camera.front.x, 0.0f, camera.front.z);
  if (glm::length(flat_front) > 0.001f) {
    flat_front = glm::normalize(flat_front);
  }
  glm::vec3 flat_right = glm::normalize(glm::cross(flat_front, glm::vec3(0, 1, 0)));

  // Build desired horizontal velocity from input
  glm::vec3 wish_dir(0.0f);
  if (input.forward)  wish_dir += flat_front;
  if (input.backward) wish_dir -= flat_front;
  if (input.left)     wish_dir -= flat_right;
  if (input.right)    wish_dir += flat_right;

  if (glm::length(wish_dir) > 0.001f) {
    wish_dir = glm::normalize(wish_dir);
    velocity.x = wish_dir.x * speed;
    velocity.z = wish_dir.z * speed;
  }

  // Jump
  if (input.jump) {
    if (underwater) {
      // In water: swim upward
      velocity.y = kPlayerWaterJumpStrength;
    } else if (at_water_surface) {
      // At water surface (feet in water, head above): boost out
      velocity.y = kPlayerJumpStrength;
    } else if (on_ground) {
      // On ground: normal jump
      velocity.y = kPlayerJumpStrength;
      on_ground = false;
    }
  }
}

void Player::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
  camera.processMouseMovement(xoffset, yoffset, constrainPitch);
}

// ─── Water Detection ───────────────────────────────────────────────────

void Player::checkWater(World* world) {
  if (!world) {
    underwater = false;
    at_water_surface = false;
    return;
  }
  // Check at eye level for underwater visual effect
  BlockType eyeBlock = world->getBlockAt(getEyePosition());
  underwater = isLiquid(eyeBlock);

  // Check at feet level for water surface detection (feet in water, head above)
  BlockType feetBlock = world->getBlockAt(position + glm::vec3(0.0f, 0.3f, 0.0f));
  at_water_surface = !underwater && isLiquid(feetBlock);
}

// ─── Auto-Step ─────────────────────────────────────────────────────────

bool Player::tryAutoStep(World* world, glm::vec3& pos, float /*old_axis*/, int /*axis*/) const {
  // Try stepping up by increments up to kPlayerStepHeight.
  // If the player can fit at the stepped-up position, accept it.
  glm::vec3 stepped = pos;
  stepped.y += kPlayerStepHeight;

  // Check that the stepped-up position is clear
  if (!collidesWithWorld(world, stepped)) {
    // Find the actual ground level by stepping back down
    for (float dy = kPlayerStepHeight; dy >= 0.0f; dy -= 0.05f) {
      glm::vec3 test = pos;
      test.y += dy;
      if (!collidesWithWorld(world, test)) {
        pos.y += dy;
        return true;
      }
    }
  }

  return false;
}

// ─── Collision Detection ───────────────────────────────────────────────

bool Player::isSolidAt(World* world, int bx, int by, int bz) const {
  if (!world) return false;
  if (by < 0 || by >= kChunkHeight) return false;

  BlockType block = world->getBlockAt(glm::vec3(bx + 0.5f, by + 0.5f, bz + 0.5f));
  return block != BlockType::AIR && !isLiquid(block);
}

bool Player::collidesWithWorld(World* world, glm::vec3 pos) const {
  if (!world) return false;

  float height = getPlayerHeight();

  // Player AABB bounds
  float minX = pos.x - kPlayerHalfWidth;
  float maxX = pos.x + kPlayerHalfWidth;
  float minY = pos.y;
  float maxY = pos.y + height;
  float minZ = pos.z - kPlayerHalfWidth;
  float maxZ = pos.z + kPlayerHalfWidth;

  // Check all blocks that overlap the player AABB
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
          // Block occupies [bx, bx+1] x [by, by+1] x [bz, bz+1]
          // Check actual AABB overlap (not just grid overlap)
          float blockMinX = static_cast<float>(bx);
          float blockMaxX = static_cast<float>(bx + 1);
          float blockMinY = static_cast<float>(by);
          float blockMaxY = static_cast<float>(by + 1);
          float blockMinZ = static_cast<float>(bz);
          float blockMaxZ = static_cast<float>(bz + 1);

          bool overlaps =
              minX < blockMaxX && maxX > blockMinX &&
              minY < blockMaxY && maxY > blockMinY &&
              minZ < blockMaxZ && maxZ > blockMinZ;

          if (overlaps) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void Player::resolveCollisions(World* world, glm::vec3 old_pos) {
  // This method is reserved for more advanced collision resolution
  // Currently using the per-axis approach in update()
  (void)world;
  (void)old_pos;
}
