#pragma once

#include "core/constants.hpp"
#include "inventory/inventory.hpp"
#include "item/recipes.hpp"
#include "vitals/vitals.hpp"
#include "sim/chat.hpp"
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
  SeenItems seen;          // every item they've ever had: unlocks recipes
  SeenItems seen_sent;     // the part of `seen` their client has been told
  std::optional<glm::ivec3> open_container; // the furnace they're looking in
  struct Breaking { glm::ivec3 pos{0}; uint64_t since_ms = 0; };
  std::optional<Breaking> breaking;         // from StartBreak (survival)
  // A line out: its bobber, when the next bite comes (0 until the bobber is
  // in water), and when the current bite's window closes (0: no bite).
  struct Fishing { EntityId bobber = 0; uint64_t bite_at_ms = 0; uint64_t window_end_ms = 0; };
  std::optional<Fishing> fishing;
  // The foot of the bed they respawn at, and of the bed they're lying in.
  std::optional<glm::ivec3> bed;
  std::optional<glm::ivec3> sleeping_in;
  int sleep_ms = 0;        // time in bed, toward skipping the night
  int sleep_health = 0;    // health when last checked in bed: losing any wakes them
  std::optional<SleepState> sleep_sent; // last SleepState sent to the owner

  Vitals vitals;
  FallTracker fall;
  Activity activity;       // from the latest MovePlayer
  int dead_ms = 0;         // time spent dead, toward respawn
  int attack_cooldown_ms = 0;
  std::optional<VitalsChanged> vitals_sent; // last snapshot sent to the owner
  // For the death line in chat: what last hurt them ("fell too far",
  // "was beaten by a Zombie"), and whether this death has been told.
  std::string hurt_by;
  bool death_told = false;
  ChatLimiter chat_limit;

  float height() const { return crouch ? kPlayerCrouchHeight : kPlayerHeight; }
  glm::vec3 eyePosition() const {
    return position +
           glm::vec3(0.0f, crouch ? kPlayerCrouchEyeHeight : kPlayerEyeHeight, 0.0f);
  }
};
