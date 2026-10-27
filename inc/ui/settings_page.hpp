#pragma once

#include "core/settings.hpp"

// The settings page, shared by the title screen and the pause menu. It edits
// the settings in place, so changes show at once; leaving it (Done, or Esc via
// close()) writes voxel.properties so they survive a restart.
class SettingsPage {
public:
  // Render distance choices, in chunks.
  static constexpr int kMinChunks = 4;
  static constexpr int kMaxChunks = 24;

  // Draws the page as a centred panel. `difficulty_locked`: a guest can't
  // change the host's difficulty. True once the player pressed Done.
  bool draw(Settings &s, bool difficulty_locked);
  // Leaving without Done (Esc): saves the same way.
  void close();
};
