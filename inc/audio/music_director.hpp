#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>

// Which track should be playing and how loud, day or night. Pure logic: the
// Audio class does the playing, so this can be tested without a sound device.
enum class MusicTrack { Day = 0, Night = 1 };
inline constexpr std::size_t kMusicTrackCount = 2;

// 06:00 up to 18:00, the daylight enemies and animals go by.
// day_fraction: 0..1, 0.5 = noon.
MusicTrack musicTrackAt(float day_fraction);

struct MusicInput {
  float dt = 0.0f;           // seconds since the last update
  bool in_world = false;     // the title screen is silent
  float day_fraction = 0.5f;
  float volume = 1.0f;       // 0..1; at 0 nothing plays at all
  std::array<bool, kMusicTrackCount> ended{}; // reached the end of the file
};

struct MusicOutput {
  std::array<bool, kMusicTrackCount> start{}; // begin from the top
  std::array<bool, kMusicTrackCount> stop{};
  std::array<float, kMusicTrackCount> gain{}; // final volume, 0..1
};

class MusicDirector {
public:
  static constexpr float kCrossfadeSeconds = 4.0f;
  static constexpr float kFadeInSeconds = 2.0f;   // from silence
  static constexpr float kLeaveFadeSeconds = 1.0f;
  static constexpr float kMinGapSeconds = 60.0f;  // silence between plays
  static constexpr float kMaxGapSeconds = 120.0f;

  explicit MusicDirector(uint32_t seed = std::random_device{}()) : rng(seed) {}

  MusicOutput update(const MusicInput &in);

  // In the silence after a track ended (for tests).
  std::optional<float> gapLeft() const { return gap; }

private:
  std::array<bool, kMusicTrackCount> playing{};
  std::array<float, kMusicTrackCount> gain{};
  std::optional<MusicTrack> wanted;
  std::optional<float> gap;
  float fade_in_seconds = kFadeInSeconds;
  std::mt19937 rng;
};
