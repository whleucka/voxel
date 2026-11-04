#include "audio/footstep_director.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>

SoundEvent FootstepDirector::sound(State &s, const Walker &w, StepSet set, float volume) {
  const Effect effect = stepEffect(set);
  // Never the same recording twice in a row.
  const int variant = pickVariant(effect, effect == s.last_effect ? s.last_variant : -1, rng);
  s.last_effect = effect;
  s.last_variant = variant;

  SoundEvent e;
  e.effect = effect;
  e.variant = variant;
  e.position = w.position;
  e.positional = !w.local;
  e.volume = volume * (w.crouch ? kCrouchVolume : 1.0f) *
             std::uniform_real_distribution<float>(0.9f, 1.1f)(rng);
  e.pitch = std::uniform_real_distribution<float>(0.94f, 1.06f)(rng) *
            (set == StepSet::Dirt ? kDirtPitch : 1.0f);
  return e;
}

std::vector<SoundEvent> FootstepDirector::update(const FootstepInput &in) {
  std::vector<SoundEvent> out;
  std::unordered_set<uint32_t> seen;
  for (const Walker &w : in.walkers) {
    seen.insert(w.id);
    auto [it, fresh] = walkers.try_emplace(w.id);
    State &s = it->second;
    const glm::vec3 delta = w.position - s.last;
    if (fresh || glm::length(delta) > kTeleport) {
      s = State{};
      s.last = w.position;
      s.airborne = !w.on_ground;
      s.peak = w.position.y;
      continue;
    }
    s.last = w.position;

    const bool audible = w.local || glm::distance(w.position, in.listener) <= kHearing;
    const bool quiet = w.fly || w.in_water || w.riding || w.sleeping;

    // Ladders: a wood step for every block climbed, up or down.
    if (w.climbing && !quiet) {
      s.airborne = false;
      s.climbed += std::abs(delta.y);
      if (s.climbed >= kClimbStep) {
        s.climbed = std::fmod(s.climbed, kClimbStep);
        if (audible) out.push_back(sound(s, w, StepSet::Wood, 1.0f));
      }
      continue;
    }
    s.climbed = kClimbStep - kFirstStep;

    // In the air: how long and from how high, for the landing.
    if (!w.on_ground) {
      if (quiet) {
        s.airborne = false;
        continue;
      }
      if (!s.airborne) {
        s.airborne = true;
        s.air_time = 0.0f;
        s.peak = w.position.y;
      }
      s.air_time += in.dt;
      s.peak = std::max(s.peak, w.position.y);
      continue;
    }

    if (s.airborne) {
      s.airborne = false;
      const float drop = s.peak - w.position.y;
      if (w.local && !quiet && (s.air_time >= kMinLandAir || drop >= kMinLandDrop)) {
        const float t = std::clamp((drop - kMinLandDrop) / (kLoudFall - kMinLandDrop), 0.0f, 1.0f);
        if (w.ground != StepSet::Silent)
          out.push_back(sound(s, w, w.ground, kJumpLandVolume + (1.0f - kJumpLandVolume) * t));
        s.stride = 0.0f; // the landing was a step
        s.still = 0.0f;
        continue;
      }
    }
    if (quiet) continue;

    // Walking: a step every stride; standing still starts the count again.
    const float walked = glm::length(glm::vec2(delta.x, delta.z));
    if (walked <= 0.2f * in.dt) {
      s.still += in.dt;
      if (s.still >= kStillSeconds) s.stride = kStride - kFirstStep;
      continue;
    }
    s.still = 0.0f;
    s.stride += walked;
    if (s.stride >= kStride) {
      s.stride = std::min(s.stride - kStride, kStride);
      if (audible && w.ground != StepSet::Silent) out.push_back(sound(s, w, w.ground, 1.0f));
    }
  }
  std::erase_if(walkers, [&](const auto &kv) { return !seen.contains(kv.first); });
  return out;
}
