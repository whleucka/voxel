#include "audio/effects.hpp"
#include "block/crop.hpp"
#include "block/plant.hpp"

int pickVariant(Effect e, int last, std::mt19937 &rng) {
  const int count = variantCount(e);
  if (count < 2 || last < 0 || last >= count)
    return std::uniform_int_distribution<int>(0, count - 1)(rng);
  // Skip over the last one.
  int v = std::uniform_int_distribution<int>(0, count - 2)(rng);
  return v >= last ? v + 1 : v;
}

MineSet mineSet(BlockType b) {
  if (b == BlockType::GLASS || b == BlockType::GLOWSTONE || b == BlockType::GLOW_CRYSTAL)
    return MineSet::Glass;
  if (isTorch(b)) return MineSet::Wood; // a stick
  switch (stepSurface(b)) {
  case StepSet::Stone: return MineSet::Stone;
  case StepSet::Wood: return MineSet::Wood;
  case StepSet::Grass:
  case StepSet::Dirt:
  case StepSet::Snow:
  case StepSet::Cloth: return MineSet::Soft;
  case StepSet::Silent: return MineSet::Silent;
  }
  return MineSet::Silent;
}

Effect hitEffect(MineSet m) {
  switch (m) {
  case MineSet::Wood: return Effect::HitWood;
  case MineSet::Glass: return Effect::HitGlass;
  case MineSet::Soft: return Effect::HitSoft;
  default: return Effect::MineStone;
  }
}

Effect breakEffect(MineSet m) {
  switch (m) {
  case MineSet::Wood: return Effect::BreakWood;
  case MineSet::Glass: return Effect::BreakGlass;
  case MineSet::Soft: return Effect::BreakSoft;
  default: return Effect::MineStone;
  }
}

bool quietBreak(BlockType b) { return isPlant(b) || isCrop(b) || isTorch(b); }

std::optional<SoundEvent> CueSounds::sound(const SoundCue &cue, glm::vec3 listener) {
  if (glm::distance(cue.position, listener) > kHearing) return std::nullopt;
  auto pick = [&](Effect effect) {
    SoundEvent e;
    e.effect = effect;
    int &prev = last[static_cast<std::size_t>(effect)];
    e.variant = prev = pickVariant(effect, prev, rng);
    e.position = cue.position;
    e.positional = true;
    return e;
  };
  auto wobble = [&] { return std::uniform_real_distribution<float>(0.95f, 1.05f)(rng); };
  if (cue.cue == Cue::BoatPlace) {
    SoundEvent e = pick(Effect::HitWood);
    e.pitch = kPlacePitch * wobble();
    return e;
  }
  if (cue.cue == Cue::BlockPlace) {
    const MineSet set = mineSet(cue.block);
    if (set == MineSet::Silent) return std::nullopt;
    SoundEvent e = pick(hitEffect(set));
    e.volume = (set == MineSet::Stone ? kStonePlaceVolume : 1.0f) *
               (quietBreak(cue.block) ? kQuietBreakVolume : 1.0f);
    e.pitch = kPlacePitch * wobble();
    return e;
  }
  if (cue.cue == Cue::BlockBreak) {
    const MineSet set = mineSet(cue.block);
    if (set == MineSet::Silent) return std::nullopt;
    SoundEvent e;
    e.effect = breakEffect(set);
    int &prev = last[static_cast<std::size_t>(e.effect)];
    e.variant = prev = pickVariant(e.effect, prev, rng);
    e.position = cue.position;
    e.positional = true;
    e.volume = quietBreak(cue.block) ? kQuietBreakVolume : 1.0f;
    e.pitch = (set == MineSet::Stone ? kStoneBreakPitch : 1.0f) *
              std::uniform_real_distribution<float>(0.95f, 1.05f)(rng);
    return e;
  }
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

std::optional<SoundEvent> MiningSounds::update(std::optional<MiningNow> now, float dt) {
  const MineSet set = now ? mineSet(now->block) : MineSet::Silent;
  if (!now || set == MineSet::Silent) {
    pos.reset();
    return std::nullopt;
  }
  if (pos != now->pos) {
    pos = now->pos; // a new block: hit at once
    until_hit = 0.0f;
  } else {
    until_hit -= dt;
  }
  if (until_hit > 0.0f) return std::nullopt;
  until_hit += kHitSeconds;
  if (until_hit <= 0.0f) until_hit = kHitSeconds; // a long frame: don't catch up

  SoundEvent e;
  e.effect = hitEffect(set);
  e.variant = pickVariant(e.effect, e.effect == last_effect ? last_variant : -1, rng);
  last_effect = e.effect;
  last_variant = e.variant;
  e.position = glm::vec3(now->pos) + 0.5f;
  e.positional = true;
  e.volume = set == MineSet::Stone ? kStoneHitVolume : 1.0f;
  e.pitch = std::uniform_real_distribution<float>(0.95f, 1.05f)(rng);
  return e;
}
