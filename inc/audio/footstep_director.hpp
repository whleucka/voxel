#pragma once

#include "audio/step_surface.hpp"
#include <array>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

// How many recordings each step set has, in StepSet order. For now every set
// shares the same ten (see footstepPaths).
inline constexpr std::array<int, kStepSetCount> kStepVariants = {10, 10, 10, 10, 10, 10};

// One player who might be stepping, this frame.
struct Walker {
  uint32_t id = 0;
  bool local = false;       // ours: not placed in 3D, and lands audibly
  glm::vec3 position{0.0f}; // feet
  bool on_ground = false;   // remote: a solid block under their feet
  bool crouch = false;
  bool fly = false;
  bool in_water = false;    // feet in water
  bool riding = false;
  bool sleeping = false;
  bool climbing = false;    // on a ladder
  StepSet ground = StepSet::Silent; // the block under their feet
};

struct FootstepInput {
  float dt = 0.0f;
  glm::vec3 listener{0.0f}; // the camera
  std::vector<Walker> walkers;
};

struct SoundEvent {
  StepSet set = StepSet::Grass;
  int variant = 0;          // 0..kStepVariants[set]-1
  glm::vec3 position{0.0f};
  bool positional = false;  // false: played as if at the listener
  float volume = 1.0f;      // before the effects volume
  float pitch = 1.0f;
};

// When footsteps, landings and ladder steps play. Pure logic: Audio does the
// playing, so this is tested without a sound device.
class FootstepDirector {
public:
  static constexpr float kStride = 1.7f;        // blocks walked per step
  static constexpr float kFirstStep = 0.5f;     // from standing
  static constexpr float kStillSeconds = 0.25f; // stopped this long: standing
  static constexpr float kClimbStep = 1.0f;     // blocks climbed per step
  static constexpr float kHearing = 16.0f;      // others, in blocks
  static constexpr float kTeleport = 4.0f;      // a jump this far in a frame
  static constexpr float kCrouchVolume = 0.5f;
  static constexpr float kJumpLandVolume = 0.6f;
  static constexpr float kMinLandAir = 0.5f;    // seconds in the air
  static constexpr float kMinLandDrop = 1.0f;   // or blocks fallen
  static constexpr float kLoudFall = 4.0f;      // full volume from here

  explicit FootstepDirector(uint32_t seed = std::random_device{}()) : rng(seed) {}

  std::vector<SoundEvent> update(const FootstepInput &in);
  std::size_t tracked() const { return walkers.size(); }

private:
  struct State {
    glm::vec3 last{0.0f};
    float stride = kStride - kFirstStep; // distance toward the next step
    float still = 0.0f;
    float climbed = kClimbStep - kFirstStep;
    bool airborne = false;
    float air_time = 0.0f;
    float peak = 0.0f;   // highest feet height since leaving the ground
    int last_variant = -1;
    StepSet last_set = StepSet::Silent;
  };
  std::unordered_map<uint32_t, State> walkers;
  std::mt19937 rng;

  SoundEvent sound(State &s, const Walker &w, StepSet set, float volume);
};
