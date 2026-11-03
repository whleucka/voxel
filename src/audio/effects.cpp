#include "audio/effects.hpp"

int pickVariant(Effect e, int last, std::mt19937 &rng) {
  const int count = variantCount(e);
  if (count < 2 || last < 0 || last >= count)
    return std::uniform_int_distribution<int>(0, count - 1)(rng);
  // Skip over the last one.
  int v = std::uniform_int_distribution<int>(0, count - 2)(rng);
  return v >= last ? v + 1 : v;
}

std::optional<SoundEvent> CueSounds::sound(const SoundCue &cue, glm::vec3 listener) {
  if (glm::distance(cue.position, listener) > kHearing) return std::nullopt;
  const bool gate = cue.cue == Cue::GateOpen || cue.cue == Cue::GateClose;
  const bool open = cue.cue == Cue::DoorOpen || cue.cue == Cue::GateOpen;

  SoundEvent e;
  e.effect = open ? Effect::DoorOpen : Effect::DoorClose;
  int &prev = last[static_cast<std::size_t>(e.effect)];
  e.variant = prev = pickVariant(e.effect, prev, rng);
  e.position = cue.position;
  e.positional = true;
  e.volume = gate ? kGateVolume : 1.0f;
  e.pitch = (gate ? kGatePitch : 1.0f) * std::uniform_real_distribution<float>(0.95f, 1.05f)(rng);
  return e;
}
