#include "player/player.hpp"
#include "core/constants.hpp"

Player::Player(glm::vec3 start_pos) : position(start_pos) {
  camera.position = position;
}

void Player::update(float dt) {
  if (!fly_mode) {
    // applyGravity(dt);
  }
  // checkWater();
  camera.position = position;
}

void Player::processKeyboard(float dt, bool forward, bool backward, bool left, bool right,
                             bool jump, bool sprint, bool up, bool down) {
  if (fly_mode) {
    glm::vec3 direction = glm::normalize(camera.front);
    float speed = sprint ? kPlayerSprintSpeed : kPlayerMoveSpeed;

    if (forward) {
      position += direction * speed * dt;
    }
    if (backward) {
      position -= direction * speed * dt;
    }
    if (left) {
      position -= camera.right * speed * dt;
    }
    if (right) {
      position += camera.right * speed * dt;
    }
    if (up) {
      position.y += speed * dt;
    }
    if (down) {
      position.y -= speed * dt;
    }
  } else {
    glm::vec3 direction = glm::normalize(glm::vec3(camera.front.x, 0, camera.front.z));
    float speed = sprint ? kPlayerSprintSpeed : kPlayerMoveSpeed;

    if (forward) {
      position += direction * speed * dt;
    }
    if (backward) {
      position -= direction * speed * dt;
    }
    if (left) {
      position -= camera.right * speed * dt;
    }
    if (right) {
      position += camera.right * speed * dt;
    }
  }
}

void Player::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
  camera.processMouseMovement(xoffset, yoffset, constrainPitch);
}

void Player::applyGravity(float dt) {
  velocity.y += kGravityForce * dt;
  position += velocity * dt;
}

void Player::checkWater() {
  // TODO: implement water physics
}
