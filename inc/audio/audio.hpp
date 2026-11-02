#pragma once

#include "audio/music_director.hpp"
#include <array>
#include <memory>
#include <string>

// Whether the file exists and can be decoded.
bool audioFileReadable(const std::string &path);

// Plays what the MusicDirector asks for. If there's no sound device or a file
// can't be read it logs one line and stays silent; nothing here throws. The
// device is opened the first time a track starts, and files are streamed on
// miniaudio's own threads.
class Audio {
public:
  explicit Audio(std::array<std::string, kMusicTrackCount> track_paths);
  ~Audio();
  Audio(const Audio &) = delete;
  Audio &operator=(const Audio &) = delete;

  void apply(const MusicOutput &out);
  // Which tracks have reached the end of their file.
  std::array<bool, kMusicTrackCount> ended() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
