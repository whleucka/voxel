#pragma once

#include "audio/footstep_director.hpp"
#include "audio/music_director.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// Whether the file exists and can be decoded.
bool audioFileReadable(const std::string &path);

// The recordings for each step set, in StepSet order: as many as
// kStepVariants says, under `dir`. Every set uses the same RPG footsteps for
// now; a set with its own recordings only needs its paths changed here.
using EffectPaths = std::array<std::vector<std::string>, kStepSetCount>;
EffectPaths footstepPaths(const std::string &dir = "assets/sound-effects");

// Plays what the MusicDirector and FootstepDirector ask for. If there's no sound device or a file
// can't be read it logs one line and stays silent; nothing here throws. The
// device is opened the first time a track starts, and files are streamed on
// miniaudio's own threads.
class Audio {
public:
  static constexpr std::size_t kMaxEffects = 16; // playing at once

  explicit Audio(std::array<std::string, kMusicTrackCount> track_paths,
                 EffectPaths effect_paths = {});
  ~Audio();
  Audio(const Audio &) = delete;
  Audio &operator=(const Audio &) = delete;

  void apply(const MusicOutput &out);
  // Which tracks have reached the end of their file.
  std::array<bool, kMusicTrackCount> ended() const;

  // Starts decoding the effects into memory, in the background; once. Until
  // a file is ready its sounds are skipped.
  void loadEffects();
  // Where the ears are, for sounds placed in the world.
  void setListener(glm::vec3 position, glm::vec3 front);
  // At `volume` (the effects setting, 0..1) times the event's own. The
  // oldest effect stops if kMaxEffects are already playing.
  void play(const SoundEvent &event, float volume);
  std::size_t effectsPlaying() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
