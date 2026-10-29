#include "doctest/doctest.h"
#include "item/item.hpp"
#include "ui/hud_layout.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

bool overlaps(const Rect &a, const Rect &b) {
  return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

bool inside(const Rect &r, float w, float h) {
  return r.x >= 0 && r.y >= 0 && r.right() <= w && r.bottom() <= h;
}

const std::vector<std::pair<float, float>> kSizes = {
    {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}, {1024, 600},
};

} // namespace

TEST_CASE("slot size is 6% of the height, clamped") {
  CHECK(HudLayout(1920, 1080).slotSize() == doctest::Approx(64.8f));
  CHECK(HudLayout(800, 400).slotSize() == 32.0f);
  CHECK(HudLayout(3840, 2160).slotSize() == 80.0f);
}

TEST_CASE("hotbar slots run left to right along the bottom, centred") {
  for (auto [w, h] : kSizes) {
    CAPTURE(w);
    HudLayout l(w, h);
    for (int i = 0; i < kHotbarSlots; ++i) {
      Rect r = l.hotbarSlot(i);
      CHECK(inside(r, w, h));
      if (i > 0) CHECK(r.x > l.hotbarSlot(i - 1).right());
      CHECK(r.y == l.hotbarSlot(0).y);
    }
    float left = l.hotbarSlot(0).x;
    float right = w - l.hotbarSlot(kHotbarSlots - 1).right();
    CHECK(std::abs(left - right) <= 1.5f); // pixel rounding
    CHECK(l.hotbarSlot(0).bottom() > h * 0.85f);
  }
}

TEST_CASE("the bar strip, breath strip and name area stack above the hotbar") {
  for (auto [w, h] : kSizes) {
    HudLayout l(w, h);
    Rect strip = l.barStrip(), breath = l.breathStrip(), name = l.nameArea();
    CHECK(strip.bottom() <= l.hotbarSlot(0).y);
    CHECK(breath.bottom() <= strip.y);
    CHECK(name.bottom() <= breath.y);
    CHECK(breath.h == strip.h);
    CHECK(inside(strip, w, h));
    CHECK(inside(breath, w, h));
    CHECK(inside(name, w, h));
    CHECK(strip.x == l.hotbarSlot(0).x);
    CHECK(strip.right() == doctest::Approx(l.hotbarSlot(kHotbarSlots - 1).right()));
  }
}

TEST_CASE("inventory screen slots fit in the panel and never overlap") {
  for (auto [w, h] : kSizes) {
    CAPTURE(w);
    HudLayout l(w, h);
    Rect panel = l.screenPanel();
    CHECK(inside(panel, w, h));
    for (int i = 0; i < kInventorySlots; ++i) {
      Rect a = l.screenSlot(i);
      CHECK(a.x >= panel.x);
      CHECK(a.right() <= panel.right());
      CHECK(a.y >= panel.y);
      CHECK(a.bottom() <= panel.bottom());
      for (int j = i + 1; j < kInventorySlots; ++j)
        CHECK_FALSE(overlaps(a, l.screenSlot(j)));
    }
  }
}

TEST_CASE("the screen's hotbar row sits below the storage rows") {
  HudLayout l(1920, 1080);
  float lowest_storage = 0;
  for (int i = kHotbarSlots; i < kInventorySlots; ++i)
    lowest_storage = std::max(lowest_storage, l.screenSlot(i).bottom());
  for (int i = 0; i < kHotbarSlots; ++i) CHECK(l.screenSlot(i).y > lowest_storage);
  // Storage reads row by row: slot 10 top-left, 19 top-right, 20 starts row 2.
  CHECK(l.screenSlot(10).x == l.screenSlot(20).x);
  CHECK(l.screenSlot(20).y > l.screenSlot(10).y);
  CHECK(l.screenSlot(19).y == l.screenSlot(10).y);
}

TEST_CASE("hit-testing finds the slot under the cursor and nothing in gaps") {
  HudLayout l(1920, 1080);
  for (int i = 0; i < kInventorySlots; ++i) {
    Rect r = l.screenSlot(i);
    glm::vec2 c = r.center();
    CHECK(l.hitTestScreen(c.x, c.y) == i);
    CHECK(l.hitTestScreen(r.x, r.y) == i);                       // top-left edge
    CHECK(l.hitTestScreen(r.right() - 0.01f, r.bottom() - 0.01f) == i);
  }
  Rect a = l.screenSlot(10), b = l.screenSlot(11);
  CHECK_FALSE(l.hitTestScreen((a.right() + b.x) * 0.5f, a.center().y)); // gap
  CHECK_FALSE(l.hitTestScreen(5, 5));                                    // outside
}

TEST_CASE("the creative palette sits beside the inventory screen, on screen") {
  int count = static_cast<int>(paletteItems().size());
  for (auto [w, h] : kSizes) {
    CAPTURE(w);
    HudLayout l(w, h);
    Rect panel = l.palettePanel(count);
    CHECK(inside(panel, w, h));
    CHECK_FALSE(overlaps(panel, l.screenPanel()));
    for (int i = 0; i < count; ++i) {
      Rect r = l.paletteSlot(i, count);
      CHECK(r.x >= panel.x);
      CHECK(r.right() <= panel.right());
      CHECK(r.bottom() <= panel.bottom());
      CHECK(l.hitTestPalette(r.center().x, r.center().y, count) == i);
      CHECK_FALSE(l.hitTestScreen(r.center().x, r.center().y));
    }
    CHECK_FALSE(l.hitTestPalette(panel.x - 1, panel.y - 1, count));
  }
}

TEST_CASE("the palette offers every item but unbreakable blocks") {
  const auto &items = paletteItems();
  CHECK(items.size() > 30);
  for (ItemId id : items) CHECK(isValidItem(id));
  CHECK(std::find(items.begin(), items.end(), itemForBlock(BlockType::BEDROCK)) == items.end());
  CHECK(std::find(items.begin(), items.end(), itemForBlock(BlockType::WATER)) == items.end());
  CHECK(std::find(items.begin(), items.end(), ItemId::DIAMOND) != items.end());
  CHECK(std::find(items.begin(), items.end(), itemForBlock(BlockType::STONE)) != items.end());
}

TEST_CASE("the bite prompt sits above the notice, clear of it") {
  HudLayout l(1920.0f, 1080.0f);
  Rect bite = l.bitePromptArea(), notice = l.noticeArea();
  CHECK(bite.bottom() <= notice.y);
  CHECK(bite.h == notice.h);
  CHECK(bite.y > 0.0f);
}

TEST_CASE("the chest panel's 20 slots fit beside the inventory without overlapping") {
  for (auto [w, h] : {std::pair{1920.0f, 1080.0f}, std::pair{1280.0f, 720.0f},
                      std::pair{3840.0f, 2160.0f}, std::pair{1024.0f, 768.0f}}) {
    CAPTURE(w);
    HudLayout l(w, h);
    Rect panel = l.chestPanel(), inv = l.screenPanel();
    REQUIRE(l.chestCell() > 0.0f);
    CHECK(panel.right() <= inv.x);
    CHECK(panel.x >= 0.0f);
    CHECK(panel.y >= 0.0f);
    CHECK(panel.bottom() <= h);
    for (int i = 0; i < kChestSlots; ++i) {
      Rect s = l.chestSlot(i);
      CHECK(s.x >= panel.x);
      CHECK(s.right() <= panel.right());
      CHECK(s.y >= l.chestTitle().bottom());
      CHECK(s.bottom() <= panel.bottom());
      CHECK(l.hitTestChest(s.center().x, s.center().y) == i);
      for (int j = 0; j < i; ++j) {
        Rect o = l.chestSlot(j);
        bool apart = s.right() <= o.x || o.right() <= s.x || s.bottom() <= o.y || o.bottom() <= s.y;
        CHECK(apart);
      }
    }
    CHECK_FALSE(l.hitTestChest(inv.center().x, inv.center().y));
  }
}

TEST_CASE("chat sits bottom-left beside the hotbar, clear of the rest of the HUD") {
  for (auto [w, h] : kSizes) {
    CAPTURE(w);
    HudLayout l(w, h);
    for (Rect r : {l.chatArea(), l.chatPanel()}) {
      CHECK(inside(r, w, h));
      CHECK(r.w >= 250.0f); // room for a readable line
      for (int i = 0; i < kHotbarSlots; ++i) CHECK_FALSE(overlaps(r, l.hotbarSlot(i)));
      CHECK_FALSE(overlaps(r, l.barStrip()));
      CHECK_FALSE(overlaps(r, l.breathStrip()));
      CHECK_FALSE(overlaps(r, l.nameArea()));
      CHECK_FALSE(overlaps(r, l.noticeArea()));
    }
    CHECK(l.chatPanel().h > l.chatArea().h);
    CHECK(l.chatPanel().bottom() == l.chatArea().bottom());
  }
}

TEST_CASE("the minimap sits in the top-left corner, clear of the open chat") {
  for (auto [w, h] : {std::pair{1600.0f, 900.0f}, {800.0f, 600.0f}, {3840.0f, 2160.0f}}) {
    CAPTURE(w);
    HudLayout l(w, h);
    Rect map = l.minimapArea();
    CHECK(map.w == map.h);
    CHECK(map.x > 0.0f);
    CHECK(map.y > 0.0f);
    CHECK(map.right() < w * 0.5f);
    CHECK(map.bottom() < l.chatPanel().y);
  }
}
