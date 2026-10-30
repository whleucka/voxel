#include <cmath>
#include "player/player.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <algorithm>

Player::Player(glm::vec3 start_pos) {
  body.position = start_pos;
  camera.position = getEyePosition();
}

glm::vec3 Player::getEyePosition() const {
  return body.position + glm::vec3(0.0f, getPlayerEyeHeight(), 0.0f);
}

float Player::getPlayerHeight() const {
  return crouching ? kPlayerCrouchHeight : kPlayerHeight;
}

float Player::getPlayerEyeHeight() const {
  return crouching ? kPlayerCrouchEyeHeight : kPlayerEyeHeight;
}

void Player::respawnAt(glm::vec3 pos) {
  body.position = pos;
  body.velocity = glm::vec3(0.0f);
  body.on_ground = false;
  camera.position = getEyePosition();
}

void Player::sitAt(glm::vec3 seat, float eye_height) {
  body.position = seat;
  body.velocity = glm::vec3(0.0f);
  body.on_ground = true;
  body.underwater = body.at_water_surface = false;
  crouching = sprinting = false;
  camera.position = seat + glm::vec3(0.0f, eye_height, 0.0f);
}

void Player::knock(glm::vec3 velocity) {
  if (fly_mode) return;
  constexpr float kKnockbackSteerDelay = 0.2f;
  body.velocity.x += velocity.x;
  body.velocity.z += velocity.z;
  body.velocity.y = std::max(body.velocity.y, velocity.y);
  body.on_ground = false;
  knockback_left = kKnockbackSteerDelay;
}

void Player::setSelectedHotbarSlot(int slot) {
  if (slot >= 0 && slot < kHotbarSlots) {
    selected_hotbar_slot = slot;
  }
}

// ─── Update ────────────────────────────────────────────────────────────

void Player::update(float dt, World* world) {
  // Hold still until the ground here has generated: an unloaded chunk has
  // nothing to stand on, so we'd fall straight through it.
  if (world) {
    glm::vec3 p = body.position;
    int cx = static_cast<int>(std::floor(p.x / kChunkWidth));
    int cz = static_cast<int>(std::floor(p.z / kChunkDepth));
    if (!world->getChunk(cx, cz)) {
      body.velocity = glm::vec3(0.0f);
      camera.position = getEyePosition();
      return;
    }
  }
  body.height = getPlayerHeight();
  body.eye_height = getPlayerEyeHeight();
  knockback_left = std::max(0.0f, knockback_left - dt);
  body.updateWater(world);
  body.climbing = !fly_mode && body.onLadder(world);
  if (body.climbing)
    body.velocity.y = climb_input > 0   ? kLadderClimbSpeed
                      : climb_input < 0 ? -kLadderSlideSpeed
                                        : 0.0f;
  if (fly_mode) {
    body.glide(dt, world);
    body.velocity = glm::vec3(0.0f); // flying has no momentum
  } else {
    body.move(dt, world);
  }
  camera.position = getEyePosition();
}

// ─── Keyboard Processing ───────────────────────────────────────────────

void Player::processKeyboard(float /*dt*/, const PlayerInput& input) {
  if (fly_mode) {
    // Fly mode: full 3D movement along camera direction. update() moves the
    // body, stopping at blocks.
    glm::vec3 direction = glm::normalize(camera.front);
    float speed = input.sprint ? kPlayerSprintSpeed * 5.0f : kPlayerMoveSpeed * 3.0f;

    glm::vec3 wish(0.0f);
    if (input.forward)  wish += direction;
    if (input.backward) wish -= direction;
    if (input.left)     wish -= camera.right;
    if (input.right)    wish += camera.right;
    if (input.fly_up)   wish.y += 1.0f;
    if (input.fly_down) wish.y -= 1.0f;

    body.velocity = wish * speed;
    return;
  }

  // ── Walk / Sprint / Crouch mode ──

  // Crouch state
  crouching = input.crouch;
  // On a ladder, pushing any way (or jumping) climbs, crouching holds on.
  const bool pushing = input.forward || input.backward || input.left || input.right;
  climb_input = crouching ? 0 : (pushing || input.jump) ? 1 : -1;

  // Sprint: only while moving forward, not crouching, and with stamina left
  sprinting = input.sprint && sprint_allowed && input.forward && !crouching;

  // Determine movement speed
  float speed;
  if (body.underwater) {
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

  if (glm::length(wish_dir) > 0.001f && knockback_left <= 0.0f) {
    wish_dir = glm::normalize(wish_dir);
    body.velocity.x = wish_dir.x * speed;
    body.velocity.z = wish_dir.z * speed;
  }

  // Jump
  if (input.jump) {
    if (body.underwater) {
      // In water: swim upward
      body.velocity.y = kPlayerWaterJumpStrength;
    } else if (body.at_water_surface) {
      // At water surface (feet in water, head above): boost out
      body.velocity.y = kPlayerJumpStrength;
    } else if (body.on_ground) {
      // On ground: normal jump
      body.velocity.y = kPlayerJumpStrength;
      body.on_ground = false;
    }
  }
}

void Player::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
  camera.processMouseMovement(xoffset, yoffset, constrainPitch);
}
