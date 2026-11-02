#include "audio/music_director.hpp"
#include "entity/animals.hpp"
#include <algorithm>

MusicTrack musicTrackAt(float day_fraction) {
  return day_fraction >= kDaylightStart && day_fraction < kDaylightEnd ? MusicTrack::Day
                                                                       : MusicTrack::Night;
}

MusicOutput MusicDirector::update(const MusicInput &in) {
  MusicOutput out;
  const float volume = std::clamp(in.volume, 0.0f, 1.0f);

  if (volume <= 0.0f) {
    for (std::size_t t = 0; t < kMusicTrackCount; ++t) {
      out.stop[t] = playing[t];
      playing[t] = false;
      gain[t] = 0.0f;
    }
    wanted.reset();
    gap.reset();
    return out;
  }

  const float dt = std::max(in.dt, 0.0f);

  for (std::size_t t = 0; t < kMusicTrackCount; ++t) {
    if (!in.ended[t] || !playing[t]) continue;
    playing[t] = false;
    gain[t] = 0.0f;
    out.stop[t] = true;
    if (wanted && static_cast<std::size_t>(*wanted) == t) {
      std::uniform_real_distribution<float> length(kMinGapSeconds, kMaxGapSeconds);
      gap = length(rng);
    }
  }

  std::optional<MusicTrack> now_wanted;
  if (in.in_world) now_wanted = musicTrackAt(in.day_fraction);
  if (now_wanted != wanted) {
    wanted = now_wanted;
    gap.reset();
    // Only a crossfade if the other track can still be heard. Set here so a
    // track that's fading back in, with no restart, gets the same speed.
    const bool audible = std::any_of(gain.begin(), gain.end(), [](float g) { return g > 0.0f; });
    fade_in_seconds = audible ? kCrossfadeSeconds : kFadeInSeconds;
  }

  if (gap) {
    *gap -= dt;
    if (*gap <= 0.0f) {
      gap.reset();
      fade_in_seconds = kFadeInSeconds;
    }
  }

  if (wanted && !gap) {
    const auto t = static_cast<std::size_t>(*wanted);
    if (!playing[t]) {
      playing[t] = true;
      gain[t] = 0.0f;
      out.start[t] = true;
    }
  }

  const float fade_out_seconds = in.in_world ? kCrossfadeSeconds : kLeaveFadeSeconds;
  for (std::size_t t = 0; t < kMusicTrackCount; ++t) {
    if (!playing[t]) continue;
    if (wanted && static_cast<std::size_t>(*wanted) == t) {
      gain[t] = std::min(gain[t] + dt / fade_in_seconds, 1.0f);
    } else {
      gain[t] = std::max(gain[t] - dt / fade_out_seconds, 0.0f);
      if (gain[t] <= 0.0f) {
        playing[t] = false;
        out.stop[t] = true;
      }
    }
    out.gain[t] = gain[t] * volume;
  }
  return out;
}
