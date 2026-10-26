#pragma once

#include "core/constants.hpp"
#include "sim/protocol.hpp"
#include <glm/glm.hpp>

// The Game's authoritative record of one connected player. The client-side
// Player (physics, camera) reports into this via MovePlayer.
struct PlayerState {
  PlayerId id = 0;
  glm::vec3 position{0.0f}; // feet
  float yaw = -90.0f;
  float pitch = 0.0f;
  int selected_slot = 0;
  bool fly = false;
  bool crouch = false;

  float height() const { return crouch ? kPlayerCrouchHeight : kPlayerHeight; }
  glm::vec3 eyePosition() const {
    return position +
           glm::vec3(0.0f, crouch ? kPlayerCrouchEyeHeight : kPlayerEyeHeight, 0.0f);
  }
};
