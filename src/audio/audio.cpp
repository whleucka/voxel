#include "audio/audio.hpp"
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_FLAC
#include "miniaudio/miniaudio.h"
#include <iostream>
#include <optional>

bool audioFileReadable(const std::string &path) {
  ma_decoder decoder;
  if (ma_decoder_init_file(path.c_str(), nullptr, &decoder) != MA_SUCCESS) return false;
  ma_decoder_uninit(&decoder);
  return true;
}

struct Audio::Impl {
  std::array<std::string, kMusicTrackCount> paths;
  std::optional<ma_engine> engine; // once a device has opened
  bool engine_failed = false;
  std::array<ma_sound, kMusicTrackCount> sounds{};
  std::array<bool, kMusicTrackCount> loaded{};
  std::array<bool, kMusicTrackCount> checked{}; // readable[] has been worked out
  std::array<bool, kMusicTrackCount> readable{};

  bool openEngine() {
    if (engine) return true;
    if (engine_failed) return false;
    engine.emplace();
    if (ma_engine_init(nullptr, &*engine) != MA_SUCCESS) {
      engine.reset();
      engine_failed = true;
      std::cerr << "[audio] no sound device; music is off\n";
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

  ~Impl() {
    for (std::size_t t = 0; t < kMusicTrackCount; ++t) unload(t);
    if (engine) ma_engine_uninit(&*engine);
  }
};

Audio::Audio(std::array<std::string, kMusicTrackCount> track_paths)
    : impl(std::make_unique<Impl>()) {
  impl->paths = std::move(track_paths);
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
