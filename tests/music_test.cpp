#include "doctest/doctest.h"
#include "audio/audio.hpp"
#include "audio/music_director.hpp"
#include "core/config.hpp"
#include "core/settings.hpp"
#include "ui/settings_page.hpp"
#include <filesystem>
#include <fstream>

namespace {

constexpr std::size_t kDay = 0, kNight = 1;
constexpr float kNoon = 0.5f, kMidnight = 0.0f;

MusicInput world(float dt, float fraction, float volume = 1.0f) {
  MusicInput in;
  in.dt = dt;
  in.in_world = true;
  in.day_fraction = fraction;
  in.volume = volume;
  return in;
}

// Runs for `seconds`, in small steps, and returns the last output. Starts and
// stops seen on the way are added up in the two counters, if given.
MusicOutput run(MusicDirector &d, MusicInput in, float seconds,
                std::array<int, kMusicTrackCount> *starts = nullptr) {
  MusicOutput out;
  const float step = 0.05f;
  in.dt = step;
  for (float t = 0; t < seconds - step / 2; t += step) {
    out = d.update(in);
    if (starts)
      for (std::size_t i = 0; i < kMusicTrackCount; ++i) (*starts)[i] += out.start[i];
  }
  return out;
}

struct ScopedSettings {
  Settings saved = g_settings;
  ~ScopedSettings() { g_settings = saved; }
};

} // namespace

TEST_CASE("music: day runs from 06:00 up to 18:00") {
  CHECK(musicTrackAt(0.0f) == MusicTrack::Night);
  CHECK(musicTrackAt(0.2499f) == MusicTrack::Night);
  CHECK(musicTrackAt(0.25f) == MusicTrack::Day);
  CHECK(musicTrackAt(0.5f) == MusicTrack::Day);
  CHECK(musicTrackAt(0.7499f) == MusicTrack::Day);
  CHECK(musicTrackAt(0.75f) == MusicTrack::Night);
  CHECK(musicTrackAt(0.9999f) == MusicTrack::Night);
}

TEST_CASE("music: a world starts the right track and fades it in over 2 seconds") {
  MusicDirector d(1);
  auto first = d.update(world(0.0f, kMidnight));
  CHECK(first.start[kNight]);
  CHECK_FALSE(first.start[kDay]);
  CHECK(run(d, world(0, kMidnight), 1.0f).gain[kNight] == doctest::Approx(0.5f).epsilon(0.05));
  CHECK(run(d, world(0, kMidnight), 1.5f).gain[kNight] == doctest::Approx(1.0f));
}

TEST_CASE("music: crossing a boundary crossfades over 4 seconds") {
  MusicDirector d(1);
  run(d, world(0, kNoon), 3.0f);
  auto mid = d.update(world(2.0f, 0.75f));
  CHECK(mid.start[kNight]);
  CHECK(mid.gain[kDay] == doctest::Approx(0.5f));
  CHECK(mid.gain[kNight] == doctest::Approx(0.5f));
  auto end = d.update(world(2.0f, 0.75f));
  CHECK(end.stop[kDay]);
  CHECK(end.gain[kDay] == 0.0f);
  CHECK(end.gain[kNight] == doctest::Approx(1.0f));
}

TEST_CASE("music: going back mid-fade reverses without restarting or jumping") {
  MusicDirector d(1);
  run(d, world(0, kNoon), 3.0f);
  d.update(world(1.0f, kMidnight)); // day 0.75, night 0.25
  auto back = d.update(world(0.05f, kNoon));
  CHECK_FALSE(back.start[kDay]);
  CHECK_FALSE(back.stop[kDay]);
  CHECK(back.gain[kDay] == doctest::Approx(0.7625f));
  CHECK(back.gain[kNight] == doctest::Approx(0.2375f));

  // Scrubbing back and forth: only ever the two tracks, and no steps.
  MusicDirector s(1);
  std::array<float, kMusicTrackCount> last{};
  for (int i = 0; i < 400; ++i) {
    auto out = s.update(world(0.05f, (i / 7) % 2 ? kNoon : kMidnight));
    for (std::size_t t = 0; t < kMusicTrackCount; ++t) {
      CHECK(out.gain[t] - last[t] <= 0.05f / MusicDirector::kFadeInSeconds + 0.001f);
      CHECK(last[t] - out.gain[t] <= 0.05f / MusicDirector::kCrossfadeSeconds + 0.001f);
      last[t] = out.gain[t];
    }
  }
}

TEST_CASE("music: a track that ends restarts after 1 to 2 minutes of silence") {
  for (uint32_t seed : {1u, 2u, 3u, 4u, 5u}) {
    MusicDirector d(seed);
    run(d, world(0, kNoon), 3.0f);
    MusicInput end = world(0.0f, kNoon);
    end.ended[kDay] = true;
    auto out = d.update(end);
    CHECK(out.stop[kDay]);
    CHECK(out.gain[kDay] == 0.0f);
    REQUIRE(d.gapLeft());
    CHECK(*d.gapLeft() >= MusicDirector::kMinGapSeconds);
    CHECK(*d.gapLeft() <= MusicDirector::kMaxGapSeconds);

    float gap = *d.gapLeft();
    std::array<int, kMusicTrackCount> starts{};
    run(d, world(0, kNoon), gap - 0.5f, &starts);
    CHECK(starts[kDay] == 0);
    run(d, world(0, kNoon), 1.0f, &starts);
    CHECK(starts[kDay] == 1);
    // Fades back in from silence, at the slower-to-notice 2 seconds.
    CHECK(run(d, world(0, kNoon), 2.5f).gain[kDay] == doctest::Approx(1.0f));
  }
}

TEST_CASE("music: a boundary crossed during the silence starts the new track at once") {
  MusicDirector d(1);
  run(d, world(0, kNoon), 3.0f);
  MusicInput end = world(0.0f, kNoon);
  end.ended[kDay] = true;
  d.update(end);
  auto out = d.update(world(0.05f, kMidnight));
  CHECK(out.start[kNight]);
  CHECK_FALSE(d.gapLeft());
}

TEST_CASE("music: the fading-out track ending doesn't start a silence") {
  MusicDirector d(1);
  run(d, world(0, kNoon), 3.0f);
  d.update(world(1.0f, kMidnight));
  MusicInput end = world(0.05f, kMidnight);
  end.ended[kDay] = true;
  auto out = d.update(end);
  CHECK(out.stop[kDay]);
  CHECK_FALSE(d.gapLeft());
  CHECK(out.gain[kNight] > 0.0f);
}

TEST_CASE("music: leaving a world fades out in a second, and the title is silent") {
  MusicDirector d(1);
  CHECK_FALSE(d.update(MusicInput{}).start[kDay]);
  run(d, world(0, kNoon), 3.0f);
  MusicInput title;
  title.dt = 0.5f;
  CHECK(d.update(title).gain[kDay] == doctest::Approx(0.5f));
  auto out = d.update(title);
  CHECK(out.stop[kDay]);
  CHECK(out.gain[kDay] == 0.0f);
  // Coming back starts afresh.
  CHECK(d.update(world(0.05f, kNoon)).start[kDay]);
}

TEST_CASE("music: volume scales at once, and 0 stops everything") {
  MusicDirector d(1);
  run(d, world(0, kNoon), 3.0f);
  CHECK(d.update(world(0.05f, kNoon, 0.25f)).gain[kDay] == doctest::Approx(0.25f));
  auto off = d.update(world(0.05f, kNoon, 0.0f));
  CHECK(off.stop[kDay]);
  CHECK(off.gain[kDay] == 0.0f);
  auto still = d.update(world(0.05f, kNoon, 0.0f));
  CHECK_FALSE(still.start[kDay]);
  CHECK_FALSE(still.stop[kDay]);
  CHECK(d.update(world(0.05f, kNoon, 0.5f)).start[kDay]);
}

TEST_CASE("audio: a missing or bad file stays silent and doesn't throw") {
  const auto junk = std::filesystem::temp_directory_path() / "voxel_music_test_junk.mp3";
  { std::ofstream(junk) << "this is not music"; }
  CHECK_FALSE(audioFileReadable("no/such/file.mp3"));
  CHECK_FALSE(audioFileReadable(junk.string()));

  Audio audio({"no/such/file.mp3", junk.string()});
  MusicOutput out;
  out.start = {true, true};
  out.gain = {1.0f, 1.0f};
  audio.apply(out);
  audio.apply(out); // tries nothing new
  CHECK(audio.ended() == std::array<bool, kMusicTrackCount>{});
  std::filesystem::remove(junk);
}

TEST_CASE("audio: the shipped tracks can be decoded") {
  CHECK(audioFileReadable("assets/music/complex.mp3"));
  CHECK(audioFileReadable("assets/music/ethereal.mp3"));
}

TEST_CASE("credits page: Esc goes back to settings") {
  SettingsPage page;
  CHECK_FALSE(page.back());
  page.openCredits();
  CHECK(page.onCredits());
  CHECK(page.back());
  CHECK_FALSE(page.onCredits());
  CHECK_FALSE(page.back());
}

TEST_CASE("music-volume: saved, loaded, clamped, and reset") {
  ScopedSettings restore;
  const auto path = std::filesystem::temp_directory_path() / "voxel_music_test.properties";
  CHECK(Settings{}.music_volume == doctest::Approx(0.5f));

  g_settings.music_volume = 0.3f;
  REQUIRE(saveServerProperties(path.string()));
  g_settings.music_volume = 0.9f;
  loadServerProperties(path.string());
  CHECK(g_settings.music_volume == doctest::Approx(0.3f));

  const auto load = [&](const char *value) {
    { std::ofstream(path) << "music-volume=" << value << "\n"; }
    loadServerProperties(path.string());
    return g_settings.music_volume;
  };
  CHECK(load("2.5") == doctest::Approx(1.0f));
  CHECK(load("-1") == doctest::Approx(0.0f));
  g_settings.music_volume = 0.4f;
  CHECK(load("nan") == doctest::Approx(0.4f));
  CHECK(load("loud") == doctest::Approx(0.4f));

  g_settings.music_volume = 0.1f;
  CHECK(defaultsKeepingIdentity(g_settings).music_volume == doctest::Approx(0.5f));
  std::filesystem::remove(path);
}
