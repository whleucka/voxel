#pragma once

#include "ui/settings_page.hpp"

// The in-game menu on Esc: Resume, Settings, Save game, Return to title
// screen, Exit game. The state (which page, what Esc does, whether the world
// keeps running) is plain logic; draw() adds the ImGui panel.
class PauseMenu {
public:
  enum class Page { Closed, Main, Settings };
  enum class Button { Resume, Settings, Save, ReturnToTitle, Exit };
  // What the Engine has to do; Resume and Settings are handled here.
  enum class Action { None, Save, ReturnToTitle, Exit };

  Page page() const { return page_; }
  bool isOpen() const { return page_ != Page::Closed; }
  void close() { page_ = Page::Closed; }

  // Esc in game (with the inventory closed): opens the menu, backs out of
  // Settings (saving them), or resumes.
  void escape();
  Action press(Button b);

  // Only a single-player world stops while paused; a hosted or joined game
  // keeps running for the other players.
  static bool worldRuns(bool single_player, bool paused) { return !(single_player && paused); }
  // The host saves; a guest can't save someone else's world.
  static bool canSave(bool guest) { return !guest; }

  // Draws the open page over a dimmed screen. `now` times the "Saved" note.
  Action draw(bool single_player, bool guest, double now);
  // The save went through: show "Saved" for a moment.
  void saved(double now) { saved_at = now; }

private:
  Page page_ = Page::Closed;
  SettingsPage settings;
  double saved_at = -1e9;
};
