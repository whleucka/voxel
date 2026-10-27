#pragma once

// When the mouse cursor is on screen. Everywhere else in game it's hidden and
// captured for looking around. The Engine applies this once per frame, so no
// single code path can leave the cursor in the wrong state.
struct CursorState {
  bool playing = false;   // in a world (not the title screen)
  bool paused = false;    // pause menu open
  bool debug = false;     // F12 panel open
  bool inventory = false; // inventory screen open (items are clicked)
};

constexpr bool cursorFree(const CursorState &s) {
  return !s.playing || s.paused || s.debug || s.inventory;
}
