#include "player/player.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"

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

void Player::setSelectedHotbarSlot(int slot) {
  if (slot >= 0 && slot < kHotbarSlots) {
    selected_hotbar_slot = slot;
  }
}

// ─── Update ────────────────────────────────────────────────────────────

void Player::update(float dt, World* world) {
  body.height = getPlayerHeight();
  body.eye_height = getPlayerEyeHeight();
  body.updateWater(world);
  if (!fly_mode) body.move(dt, world);
  camera.position = getEyePosition();
}

// ─── Keyboard Processing ───────────────────────────────────────────────

void Player::processKeyboard(float dt, const PlayerInput& input) {
  if (fly_mode) {
    // Fly mode: full 3D movement along camera direction
    glm::vec3 direction = glm::normalize(camera.front);
    float speed = input.sprint ? kPlayerSprintSpeed * 5.0f : kPlayerMoveSpeed * 3.0f;

    if (input.forward)  body.position += direction * speed * dt;
    if (input.backward) body.position -= direction * speed * dt;
    if (input.left)     body.position -= camera.right * speed * dt;
    if (input.right)    body.position += camera.right * speed * dt;
    if (input.fly_up)   body.position.y += speed * dt;
    if (input.fly_down) body.position.y -= speed * dt;

    body.velocity = glm::vec3(0.0f); // No physics in fly mode
    return;
  }

  // ── Walk / Sprint / Crouch mode ──

  // Crouch state
  crouching = input.crouch;

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

  if (glm::length(wish_dir) > 0.001f) {
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
