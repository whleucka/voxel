#pragma once

#include "core/constants.hpp"
#include "inventory/inventory.hpp"
#include "vitals/vitals.hpp"
#include "sim/protocol.hpp"
#include <glm/glm.hpp>
#include <optional>

// The Game's authoritative record of one connected player. The client-side
// Player (physics, camera) reports into this via MovePlayer.
struct PlayerState {
  PlayerId id = 0;
  std::string name;
  uint32_t respawn = 0;    // bumped on every respawn; see MovePlayer::respawn
  glm::vec3 position{0.0f}; // feet
  float yaw = -90.0f;
  float pitch = 0.0f;
  int selected_slot = 0;
  bool fly = false;
  bool crouch = false;
  Inventory inventory;

  Vitals vitals;
  FallTracker fall;
  Activity activity;       // from the latest MovePlayer
  int dead_ms = 0;         // time spent dead, toward respawn
  int attack_cooldown_ms = 0;
  std::optional<VitalsChanged> vitals_sent; // last snapshot sent to the owner

  float height() const { return crouch ? kPlayerCrouchHeight : kPlayerHeight; }
  glm::vec3 eyePosition() const {
    return position +
           glm::vec3(0.0f, crouch ? kPlayerCrouchEyeHeight : kPlayerEyeHeight, 0.0f);
  }
};
