#include "doctest/doctest.h"
#include "core/settings.hpp"
#include "test_util.hpp"
#include "ui/pause_menu.hpp"
#include <filesystem>

using Page = PauseMenu::Page;
using Button = PauseMenu::Button;
using Action = PauseMenu::Action;

TEST_CASE("Esc opens the pause menu and Esc again resumes") {
  PauseMenu m;
  CHECK_FALSE(m.isOpen());
  m.escape();
  CHECK(m.page() == Page::Main);
  m.escape();
  CHECK(m.page() == Page::Closed);
}

TEST_CASE("Esc backs out of Settings before resuming, and saves them") {
  TempDir dir;
  auto cwd = std::filesystem::current_path();
  std::filesystem::current_path(dir.path); // settings are written to ./voxel.properties
  PauseMenu m;
  m.escape();
  CHECK(m.press(Button::Settings) == Action::None);
  CHECK(m.page() == Page::Settings);
  m.escape();
  CHECK(m.page() == Page::Main);
  CHECK(std::filesystem::exists(dir.path / "voxel.properties"));
  m.escape();
  CHECK_FALSE(m.isOpen());
  std::filesystem::current_path(cwd);
}

TEST_CASE("the buttons") {
  PauseMenu m;
  m.escape();
  CHECK(m.press(Button::Resume) == Action::None);
  CHECK_FALSE(m.isOpen());

  m.escape();
  CHECK(m.press(Button::Save) == Action::Save);
  CHECK(m.isOpen()); // stays open to show "Saved"

  CHECK(m.press(Button::ReturnToTitle) == Action::ReturnToTitle);
  CHECK_FALSE(m.isOpen());

  m.escape();
  CHECK(m.press(Button::Exit) == Action::Exit);
  CHECK_FALSE(m.isOpen());
}

TEST_CASE("only single player stops while paused") {
  CHECK_FALSE(PauseMenu::worldRuns(true, true));
  CHECK(PauseMenu::worldRuns(true, false));
  CHECK(PauseMenu::worldRuns(false, true)); // host or guest
  CHECK(PauseMenu::worldRuns(false, false));
}

TEST_CASE("guests can't save the host's world") {
  CHECK(PauseMenu::canSave(false));
  CHECK_FALSE(PauseMenu::canSave(true));
}
