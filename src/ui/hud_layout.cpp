#include "ui/hud_layout.hpp"
#include <algorithm>
#include <cmath>

namespace {

constexpr int kCols = kHotbarSlots;                  // 10 wide everywhere
constexpr int kStorageRows = kStorageSlots / kCols;  // 3

} // namespace

HudLayout::HudLayout(float display_w, float display_h)
    : w(display_w), h(display_h),
      slot(std::clamp(display_h * kSlotFraction, kMinSlot, kMaxSlot)),
      gap_(std::round(slot * 0.08f)) {}

float HudLayout::rowWidth() const { return kCols * slot + (kCols - 1) * gap_; }

// ─── In-game hotbar ────────────────────────────────────────────────────

Rect HudLayout::hotbarSlot(int i) const {
  float x0 = std::round((w - rowWidth()) * 0.5f);
  float y = h - slot - std::round(slot * 0.3f);
  return {x0 + i * (slot + gap_), y, slot, slot};
}

Rect HudLayout::barStrip() const {
  Rect first = hotbarSlot(0);
  float strip_h = std::round(slot * 0.35f);
  return {first.x, first.y - 2 * gap_ - strip_h, rowWidth(), strip_h};
}

Rect HudLayout::breathStrip() const {
  Rect strip = barStrip();
  return {strip.x, strip.y - gap_ - strip.h, strip.w, strip.h};
}

Rect HudLayout::nameArea() const {
  Rect strip = breathStrip();
  float name_h = std::round(slot * 0.4f);
  return {strip.x, strip.y - gap_ - name_h, strip.w, name_h};
}

// ─── Inventory screen ──────────────────────────────────────────────────

namespace {
// Extra space between the storage rows and the hotbar row on the screen.
float separator(float slot) { return std::round(slot * 0.5f); }
float padding(float slot) { return std::round(slot * 0.4f); }
} // namespace

Rect HudLayout::screenPanel() const {
  float pad = padding(slot);
  float grid_h = (kStorageRows + 1) * slot + (kStorageRows - 1) * gap_ + separator(slot);
  float pw = rowWidth() + 2 * pad;
  float ph = grid_h + 2 * pad;
  return {std::round((w - pw) * 0.5f), std::round((h - ph) * 0.5f), pw, ph};
}

Rect HudLayout::screenSlot(int i) const {
  Rect panel = screenPanel();
  float pad = padding(slot);
  float left = panel.x + pad;
  float top = panel.y + pad;

  if (i < kHotbarSlots) {
    float y = top + kStorageRows * slot + (kStorageRows - 1) * gap_ + separator(slot);
    return {left + i * (slot + gap_), y, slot, slot};
  }
  int s = i - kHotbarSlots;
  int row = s / kCols, col = s % kCols;
  return {left + col * (slot + gap_), top + row * (slot + gap_), slot, slot};
}

std::optional<int> HudLayout::hitTestScreen(float px, float py) const {
  for (int i = 0; i < kInventorySlots; ++i)
    if (screenSlot(i).contains(px, py)) return i;
  return std::nullopt;
}

// ─── Creative palette ──────────────────────────────────────────────────

Rect HudLayout::palettePanel(int count) const {
  Rect inv = screenPanel();
  float pad = padding(slot);
  int rows = (count + kPaletteCols - 1) / kPaletteCols;
  float pw = kPaletteCols * slot + (kPaletteCols - 1) * gap_ + 2 * pad;
  float ph = rows * slot + std::max(0, rows - 1) * gap_ + 2 * pad;
  return {inv.right() + pad, std::round((h - ph) * 0.5f), pw, ph};
}

Rect HudLayout::paletteSlot(int i, int count) const {
  Rect panel = palettePanel(count);
  float pad = padding(slot);
  int row = i / kPaletteCols, col = i % kPaletteCols;
  return {panel.x + pad + col * (slot + gap_), panel.y + pad + row * (slot + gap_), slot, slot};
}

std::optional<int> HudLayout::hitTestPalette(float px, float py, int count) const {
  for (int i = 0; i < count; ++i)
    if (paletteSlot(i, count).contains(px, py)) return i;
  return std::nullopt;
}
