#include "doctest/doctest.h"
#include "ui/hud_layout.hpp"
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

TEST_CASE("the bar strip and name area stack above the hotbar") {
  for (auto [w, h] : kSizes) {
    HudLayout l(w, h);
    Rect strip = l.barStrip(), name = l.nameArea();
    CHECK(strip.bottom() <= l.hotbarSlot(0).y);
    CHECK(name.bottom() <= strip.y);
    CHECK(inside(strip, w, h));
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
