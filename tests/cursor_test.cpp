#include "doctest/doctest.h"
#include "ui/cursor.hpp"

TEST_CASE("the cursor is captured in game, and free only for menus") {
  CHECK(cursorFree({}));                             // title screen
  CHECK_FALSE(cursorFree({true, false, false, false})); // playing
  CHECK(cursorFree({true, true, false, false}));     // pause menu
  CHECK(cursorFree({true, false, true, false}));     // F12 panel
  CHECK(cursorFree({true, false, false, true}));     // inventory
}
