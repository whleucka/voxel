#pragma once

#include "audio/step_surface.hpp"
#include "sim/protocol.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <glm/glm.hpp>

// Every group of recordings Audio can play. The steps come first, in StepSet
// order.
enum class Effect : uint8_t {
  StepGrass = 0, StepDirt, StepStone, StepWood, StepSnow, StepCloth,
  DoorOpen, DoorClose,
};
inline constexpr std::size_t kEffectCount = 8;

// How many recordings each effect has. Every step set shares the same ten
// for now (see effectPaths).
inline constexpr std::array<int, kEffectCount> kEffectVariants = {10, 10, 10, 10, 10, 10, 2, 4};

inline Effect stepEffect(StepSet s) { return static_cast<Effect>(s); }
inline int variantCount(Effect e) { return kEffectVariants[static_cast<std::size_t>(e)]; }

struct SoundEvent {
  Effect effect = Effect::StepGrass;
  int variant = 0;          // 0..variantCount(effect)-1
  glm::vec3 position{0.0f};
  bool positional = false;  // false: played as if at the listener
  float volume = 1.0f;      // before the effects volume
  float pitch = 1.0f;
};

// A random recording of `e`, never `last` again (when it has more than one).
int pickVariant(Effect e, int last, std::mt19937 &rng);

// What the Game's SoundCues sound like, heard from `listener`.
class CueSounds {
public:
  static constexpr float kHearing = 16.0f;
  static constexpr float kGatePitch = 1.15f;
  static constexpr float kGateVolume = 0.7f;

  explicit CueSounds(uint32_t seed = std::random_device{}()) : rng(seed) {}

  // Nothing for a cue too far away to hear.
  std::optional<SoundEvent> sound(const SoundCue &cue, glm::vec3 listener);

private:
  std::array<int, kEffectCount> last{-1, -1, -1, -1, -1, -1, -1, -1};
  std::mt19937 rng;
};
