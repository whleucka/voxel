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
  MineStone, // hits and breaks: the break is pitched down
  HitWood, BreakWood, HitGlass, BreakGlass, HitSoft, BreakSoft,
};
inline constexpr std::size_t kEffectCount = 15;

// How many recordings each effect has. Dirt shares grass's, a little lower
// (see effectPaths and FootstepDirector).
inline constexpr std::array<int, kEffectCount> kEffectVariants = {5, 5, 5, 5, 5, 5, 2, 4,
                                                                  5,  5,  5,  5,  5,  5,  5};

inline Effect stepEffect(StepSet s) { return static_cast<Effect>(s); }

// What a block sounds like being mined and broken.
enum class MineSet { Stone, Wood, Glass, Soft, Silent };
MineSet mineSet(BlockType b);
Effect hitEffect(MineSet m);   // m isn't Silent
Effect breakEffect(MineSet m); // m isn't Silent
// Picked at once (plants, crops, torches): a quieter break, and a quieter
// place.
bool quietBreak(BlockType b);
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
  static constexpr float kStoneBreakPitch = 0.85f;
  static constexpr float kQuietBreakVolume = 0.6f;
  static constexpr float kPlacePitch = 0.9f;
  static constexpr float kStonePlaceVolume = 0.6f; // as loud as a hit

  explicit CueSounds(uint32_t seed = std::random_device{}()) : rng(seed) {}

  // Nothing for a cue too far away to hear.
  std::optional<SoundEvent> sound(const SoundCue &cue, glm::vec3 listener);

private:
  std::array<int, kEffectCount> last = lastNone();
  std::mt19937 rng;
  static std::array<int, kEffectCount> lastNone() {
    std::array<int, kEffectCount> a;
    a.fill(-1);
    return a;
  }
};

// The block being mined, while it's breaking.
struct MiningNow {
  glm::ivec3 pos{0};
  BlockType block = BlockType::AIR;
};

// When the hits play while you mine: one at once, then every kHitSeconds.
// Pure logic, tested without a sound device.
class MiningSounds {
public:
  static constexpr float kHitSeconds = 0.25f;
  static constexpr float kStoneHitVolume = 0.6f;

  explicit MiningSounds(uint32_t seed = std::random_device{}()) : rng(seed) {}

  // `now`: what's breaking this frame, or nothing.
  std::optional<SoundEvent> update(std::optional<MiningNow> now, float dt);

private:
  std::optional<glm::ivec3> pos;
  float until_hit = 0.0f;
  int last_variant = -1;
  Effect last_effect = Effect::MineStone;
  std::mt19937 rng;
};
