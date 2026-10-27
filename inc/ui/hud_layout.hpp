#pragma once

#include "inventory/inventory.hpp"
#include <glm/glm.hpp>
#include <optional>

// Axis-aligned screen rectangle in display coordinates (origin top-left).
struct Rect {
  float x = 0, y = 0, w = 0, h = 0;

  float right() const { return x + w; }
  float bottom() const { return y + h; }
  glm::vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }
  // Half-open, so neighbouring rects never both claim a point.
  bool contains(float px, float py) const {
    return px >= x && px < right() && py >= y && py < bottom();
  }
};

// Where every HUD element goes for a given display size. Pure geometry: the
// Hud draws with it and the inventory screen hit-tests with it, so drawing and
// clicking always agree.
class HudLayout {
public:
  static constexpr float kSlotFraction = 0.06f; // of display height
  static constexpr float kMinSlot = 32.0f;
  static constexpr float kMaxSlot = 80.0f;

  HudLayout(float display_w, float display_h);

  float slotSize() const { return slot; }
  float gap() const { return gap_; }

  // ─── In-game hotbar ─────────────────────────────────────────────────
  Rect hotbarSlot(int i) const;  // 0..kHotbarSlots-1, bottom centre
  Rect barStrip() const;         // hearts (left) and stamina bolts (right)
  Rect breathStrip() const;      // breath bubbles, above the bar strip
  Rect nameArea() const;         // held-item name popup, above the breath strip
  Rect noticeArea() const;       // "New recipe!" popup, above the name popup

  // ─── Inventory screen ───────────────────────────────────────────────
  Rect screenPanel() const;
  // Any inventory slot: storage rows on top, the hotbar row at the bottom.
  Rect screenSlot(int i) const;
  std::optional<int> hitTestScreen(float px, float py) const;

  // ─── Creative palette (right of the inventory screen) ───────────────
  // At least this many columns; more, and then smaller cells, when the items
  // wouldn't otherwise fit between the inventory and the screen's edges.
  static constexpr int kPaletteCols = 5;
  Rect palettePanel(int count) const;
  Rect paletteSlot(int i, int count) const; // 0..count-1, row by row
  std::optional<int> hitTestPalette(float px, float py, int count) const;

  // ─── Crafting (left of the inventory screen) ────────────────────────
  // The panel holds a title line ("Crafting", how many are still hidden)
  // and as many recipe rows as fit; the rest scroll.
  Rect craftingPanel() const;
  Rect craftingTitle() const;
  float craftingRowHeight() const;
  int craftingVisibleRows() const;
  // The visible row `i` (0 = the top one on screen).
  Rect craftingRow(int i) const;
  // Which visible row is under the point.
  std::optional<int> hitTestCrafting(float px, float py) const;

  // ─── Furnace (in the crafting panel's place while one is open) ──────
  Rect furnacePanel() const;
  Rect furnaceTitle() const;
  // 0 = input, 1 = fuel, 2 = output.
  Rect furnaceSlot(int i) const;
  Rect furnaceArrow() const; // cooking progress, input -> output
  Rect furnaceFlame() const; // fuel left, beside the fuel slot
  std::optional<int> hitTestFurnace(float px, float py) const;

private:
  float w, h;
  struct PaletteGrid { int cols; float cell; };
  PaletteGrid paletteGrid(int count) const;
  float slot, gap_;
  float rowWidth() const;
};
