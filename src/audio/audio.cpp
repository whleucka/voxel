#include "audio/audio.hpp"
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_FLAC
#include "miniaudio/miniaudio.h"
#include <cstdio>
#include <iostream>
#include <optional>

bool audioFileReadable(const std::string &path) {
  ma_decoder decoder;
  if (ma_decoder_init_file(path.c_str(), nullptr, &decoder) != MA_SUCCESS) return false;
  ma_decoder_uninit(&decoder);
  return true;
}

EffectPaths effectPaths(const std::string &dir) {
  EffectPaths paths;
  for (std::size_t e = 0; e < kEffectCount; ++e)
    for (int v = 0; v < kEffectVariants[e]; ++v) {
      char name[64];
      switch (static_cast<Effect>(e)) {
      case Effect::DoorOpen: std::snprintf(name, sizeof name, "rpg/doorOpen_%d.ogg", v + 1); break;
      case Effect::DoorClose: std::snprintf(name, sizeof name, "rpg/doorClose_%d.ogg", v + 1); break;
      case Effect::MineStone: std::snprintf(name, sizeof name, "impact/impactMining_%03d.ogg", v); break;
      case Effect::HitWood: std::snprintf(name, sizeof name, "impact/impactWood_medium_%03d.ogg", v); break;
      case Effect::BreakWood: std::snprintf(name, sizeof name, "impact/impactWood_heavy_%03d.ogg", v); break;
      case Effect::HitGlass: std::snprintf(name, sizeof name, "impact/impactGlass_light_%03d.ogg", v); break;
      case Effect::BreakGlass: std::snprintf(name, sizeof name, "impact/impactGlass_heavy_%03d.ogg", v); break;
      case Effect::HitSoft: std::snprintf(name, sizeof name, "impact/impactSoft_medium_%03d.ogg", v); break;
      case Effect::BreakSoft: std::snprintf(name, sizeof name, "impact/impactSoft_heavy_%03d.ogg", v); break;
      case Effect::ChestOpen: std::snprintf(name, sizeof name, "rpg/creak%d.ogg", v + 1); break;
      case Effect::ChestClose: std::snprintf(name, sizeof name, "impact/impactWood_light_%03d.ogg", v); break;
      case Effect::Boom: std::snprintf(name, sizeof name, "tnt/boom_%03d.ogg", v); break;
      case Effect::Fuse: std::snprintf(name, sizeof name, "tnt/fuse_%03d.ogg", v); break;
      case Effect::StepGrass:
      case Effect::StepDirt: std::snprintf(name, sizeof name, "impact/footstep_grass_%03d.ogg", v); break;
      case Effect::StepStone: std::snprintf(name, sizeof name, "impact/footstep_concrete_%03d.ogg", v); break;
      case Effect::StepWood: std::snprintf(name, sizeof name, "impact/footstep_wood_%03d.ogg", v); break;
      case Effect::StepSnow: std::snprintf(name, sizeof name, "impact/footstep_snow_%03d.ogg", v); break;
      case Effect::StepCloth: std::snprintf(name, sizeof name, "impact/footstep_carpet_%03d.ogg", v); break;
      }
      paths[e].push_back(dir + "/" + name);
    }
  return paths;
}

struct Audio::Impl {
  std::array<std::string, kMusicTrackCount> paths;
  std::optional<ma_engine> engine; // once a device has opened
  bool engine_failed = false;
  std::array<ma_sound, kMusicTrackCount> sounds{};
  std::array<bool, kMusicTrackCount> loaded{};
  std::array<bool, kMusicTrackCount> checked{}; // readable[] has been worked out
  std::array<bool, kMusicTrackCount> readable{};

  // Effects: each file decoded once (a prototype), and played as copies that
  // share its samples.
  struct Recording {
    std::string path;
    std::unique_ptr<ma_sound> sound; // null: not loaded, or it failed
    bool failed = false;
  };
  std::array<std::vector<Recording>, kEffectCount> effects;
  bool effects_loading = false;
  struct Voice {
    ma_sound sound;
    bool used = false;
    uint64_t started = 0; // play order, to stop the oldest
  };
  std::array<Voice, kMaxEffects> voices{};
  uint64_t plays = 0;

  bool openEngine() {
    if (engine) return true;
    if (engine_failed) return false;
    engine.emplace();
    if (ma_engine_init(nullptr, &*engine) != MA_SUCCESS) {
      engine.reset();
      engine_failed = true;
      std::cerr << "[audio] no sound device; sound is off\n";
      return false;
    }
    return true;
  }

  void unload(std::size_t t) {
    if (!loaded[t]) return;
    ma_sound_uninit(&sounds[t]);
    loaded[t] = false;
  }

  // A fresh sound per play: a seek happens later on the audio thread, so a
  // reused sound would still say it had ended for a moment.
  void start(std::size_t t) {
    if (!checked[t]) {
      checked[t] = true;
      readable[t] = audioFileReadable(paths[t]);
      if (!readable[t]) std::cerr << "[audio] can't read " << paths[t] << "; skipping it\n";
    }
    if (!readable[t] || !openEngine()) return;
    unload(t);
    const ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_ASYNC |
                            MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION;
    if (ma_sound_init_from_file(&*engine, paths[t].c_str(), flags, nullptr, nullptr,
                                &sounds[t]) != MA_SUCCESS) {
      readable[t] = false;
      std::cerr << "[audio] can't play " << paths[t] << "; skipping it\n";
      return;
    }
    loaded[t] = true;
    ma_sound_set_volume(&sounds[t], 0.0f);
    ma_sound_start(&sounds[t]);
  }

  void failEffect(Recording &e) {
    if (e.sound) ma_sound_uninit(e.sound.get());
    e.sound.reset();
    e.failed = true;
    std::cerr << "[audio] can't read " << e.path << "; skipping it\n";
  }

  // The prototype for `e` if it has finished decoding.
  ma_sound *ready(Recording &e) {
    if (!e.sound) return nullptr;
    const ma_result r = ma_resource_manager_data_source_result(e.sound->pResourceManagerDataSource);
    if (r == MA_BUSY) return nullptr;
    if (r != MA_SUCCESS) {
      failEffect(e);
      return nullptr;
    }
    return e.sound.get();
  }

  void stopVoice(Voice &v) {
    if (!v.used) return;
    ma_sound_uninit(&v.sound);
    v.used = false;
  }

  // A free voice, after clearing out the finished ones; else the oldest.
  Voice &freeVoice() {
    Voice *oldest = nullptr;
    for (Voice &v : voices) {
      if (v.used && ma_sound_at_end(&v.sound)) stopVoice(v);
      if (!v.used) return v;
      if (!oldest || v.started < oldest->started) oldest = &v;
    }
    stopVoice(*oldest);
    return *oldest;
  }

  ~Impl() {
    for (Voice &v : voices) stopVoice(v);
    for (auto &set : effects)
      for (Recording &e : set)
        if (e.sound) ma_sound_uninit(e.sound.get());
    for (std::size_t t = 0; t < kMusicTrackCount; ++t) unload(t);
    if (engine) ma_engine_uninit(&*engine);
  }
};

Audio::Audio(std::array<std::string, kMusicTrackCount> track_paths, EffectPaths effect_paths)
    : impl(std::make_unique<Impl>()) {
  impl->paths = std::move(track_paths);
  for (std::size_t e = 0; e < kEffectCount; ++e)
    for (std::string &path : effect_paths[e]) impl->effects[e].push_back({std::move(path), nullptr});
}

Audio::~Audio() = default;

void Audio::apply(const MusicOutput &out) {
  for (std::size_t t = 0; t < kMusicTrackCount; ++t) {
    if (out.stop[t]) impl->unload(t);
    if (out.start[t]) impl->start(t);
    if (impl->loaded[t]) ma_sound_set_volume(&impl->sounds[t], out.gain[t]);
  }
}

std::array<bool, kMusicTrackCount> Audio::ended() const {
  std::array<bool, kMusicTrackCount> result{};
  for (std::size_t t = 0; t < kMusicTrackCount; ++t)
    result[t] = impl->loaded[t] && ma_sound_at_end(&impl->sounds[t]);
  return result;
}

void Audio::loadEffects() {
  if (impl->effects_loading) return;
  impl->effects_loading = true;
  if (!impl->openEngine()) return;
  const ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC;
  for (auto &set : impl->effects)
    for (Impl::Recording &e : set) {
      e.sound = std::make_unique<ma_sound>();
      if (ma_sound_init_from_file(&*impl->engine, e.path.c_str(), flags, nullptr, nullptr,
                                  e.sound.get()) != MA_SUCCESS) {
        e.sound.reset();
        impl->failEffect(e);
      }
    }
}

void Audio::setListener(glm::vec3 position, glm::vec3 front) {
  if (!impl->engine) return;
  ma_engine_listener_set_position(&*impl->engine, 0, position.x, position.y, position.z);
  ma_engine_listener_set_direction(&*impl->engine, 0, front.x, front.y, front.z);
}

void Audio::play(const SoundEvent &event, float volume) {
  if (volume <= 0.0f || !impl->engine) return;
  auto &set = impl->effects[static_cast<std::size_t>(event.effect)];
  if (event.variant < 0 || static_cast<std::size_t>(event.variant) >= set.size()) return;
  ma_sound *proto = impl->ready(set[event.variant]);
  if (!proto) return;

  Impl::Voice &v = impl->freeVoice();
  if (ma_sound_init_copy(&*impl->engine, proto, 0, nullptr, &v.sound) != MA_SUCCESS) return;
  v.used = true;
  v.started = ++impl->plays;
  ma_sound_set_volume(&v.sound, volume * event.volume);
  ma_sound_set_pitch(&v.sound, event.pitch);
  if (event.positional) {
    ma_sound_set_position(&v.sound, event.position.x, event.position.y, event.position.z);
    ma_sound_set_attenuation_model(&v.sound, ma_attenuation_model_linear);
    ma_sound_set_min_distance(&v.sound, 1.0f);
    ma_sound_set_max_distance(&v.sound, event.hearing > 0.0f ? event.hearing : CueSounds::kHearing);
  } else {
    ma_sound_set_spatialization_enabled(&v.sound, MA_FALSE);
  }
  ma_sound_start(&v.sound);
}

std::size_t Audio::effectsPlaying() const {
  std::size_t n = 0;
  for (const Impl::Voice &v : impl->voices) n += v.used && !ma_sound_at_end(&v.sound);
  return n;
}
