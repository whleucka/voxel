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

Rect HudLayout::noticeArea() const {
  Rect name = nameArea();
  return {name.x, name.y - gap_ - name.h, name.w, name.h};
}

Rect HudLayout::bitePromptArea() const {
  Rect notice = noticeArea();
  return {notice.x, notice.y - gap_ - notice.h, notice.w, notice.h};
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

HudLayout::PaletteGrid HudLayout::paletteGrid(int count) const {
  Rect inv = screenPanel();
  float pad = padding(slot);
  float room_w = w - inv.right() - 3 * pad; // a pad each side of the panel, one inside
  float room_h = h - 4 * pad;
  // Full-size cells in as few columns as fit; failing that, shrink them.
  for (float cell = slot; cell >= slot * 0.5f; cell -= 1.0f) {
    int max_cols = static_cast<int>((room_w + gap_) / (cell + gap_));
    int max_rows = static_cast<int>((room_h + gap_) / (cell + gap_));
    if (max_rows < 1) continue;
    int cols = std::max(kPaletteCols, (count + max_rows - 1) / max_rows);
    if (cols <= max_cols) return {cols, cell};
  }
  return {kPaletteCols, std::round(slot * 0.5f)};
}

Rect HudLayout::palettePanel(int count) const {
  Rect inv = screenPanel();
  float pad = padding(slot);
  PaletteGrid g = paletteGrid(count);
  int rows = (count + g.cols - 1) / g.cols;
  float pw = g.cols * g.cell + (g.cols - 1) * gap_ + 2 * pad;
  float ph = rows * g.cell + std::max(0, rows - 1) * gap_ + 2 * pad;
  return {inv.right() + pad, std::round((h - ph) * 0.5f), pw, ph};
}

Rect HudLayout::paletteSlot(int i, int count) const {
  Rect panel = palettePanel(count);
  float pad = padding(slot);
  PaletteGrid g = paletteGrid(count);
  int row = i / g.cols, col = i % g.cols;
  return {panel.x + pad + col * (g.cell + gap_), panel.y + pad + row * (g.cell + gap_), g.cell,
          g.cell};
}

std::optional<int> HudLayout::hitTestPalette(float px, float py, int count) const {
  for (int i = 0; i < count; ++i)
    if (paletteSlot(i, count).contains(px, py)) return i;
  return std::nullopt;
}

// ─── Crafting ──────────────────────────────────────────────────────────

Rect HudLayout::craftingPanel() const {
  Rect inv = screenPanel();
  float pad = padding(slot);
  // As wide as 6.5 slots, or whatever fits between the left edge and the
  // inventory; as tall as the screen allows, centred like the palette.
  float pw = std::min(std::round(slot * 6.5f), inv.x - 2 * pad);
  float ph = std::round(h * 0.8f);
  return {std::max(pad, inv.x - pad - pw), std::round((h - ph) * 0.5f), std::max(0.0f, pw), ph};
}

Rect HudLayout::craftingTitle() const {
  Rect panel = craftingPanel();
  float pad = padding(slot);
  return {panel.x + pad, panel.y + pad, panel.w - 2 * pad, std::round(slot * 0.45f)};
}

float HudLayout::craftingRowHeight() const { return std::round(slot * 1.05f); }

int HudLayout::craftingVisibleRows() const {
  Rect panel = craftingPanel();
  Rect title = craftingTitle();
  float pad = padding(slot);
  float room = panel.bottom() - pad - (title.bottom() + gap_);
  return std::max(0, static_cast<int>((room + gap_) / (craftingRowHeight() + gap_)));
}

Rect HudLayout::craftingRow(int i) const {
  Rect title = craftingTitle();
  float rh = craftingRowHeight();
  return {title.x, title.bottom() + gap_ + i * (rh + gap_), title.w, rh};
}

std::optional<int> HudLayout::hitTestCrafting(float px, float py) const {
  for (int i = 0; i < craftingVisibleRows(); ++i)
    if (craftingRow(i).contains(px, py)) return i;
  return std::nullopt;
}

// ─── Furnace ───────────────────────────────────────────────────────────

Rect HudLayout::furnacePanel() const {
  Rect left = craftingPanel();
  Rect inv = screenPanel();
  float pad = padding(slot);
  float ph = std::round(slot * 3.0f + 3 * gap_ + 2 * pad + slot * 0.45f);
  return {left.x, std::round(inv.center().y - ph * 0.5f), left.w, ph};
}

Rect HudLayout::furnaceTitle() const {
  Rect panel = furnacePanel();
  float pad = padding(slot);
  return {panel.x + pad, panel.y + pad, panel.w - 2 * pad, std::round(slot * 0.45f)};
}

Rect HudLayout::furnaceSlot(int i) const {
  Rect panel = furnacePanel();
  Rect title = furnaceTitle();
  // Input and output on one row with the arrow between; fuel under the input.
  float row1 = title.bottom() + gap_ * 2;
  float row2 = row1 + slot + std::round(slot * 0.6f);
  float span = std::min(panel.w - 2 * padding(slot), slot * 4.0f);
  float x0 = std::round(panel.center().x - span * 0.5f);
  if (i == 0) return {x0, row1, slot, slot};
  if (i == 1) return {x0, row2, slot, slot};
  return {x0 + span - slot, row1, slot, slot};
}

Rect HudLayout::furnaceArrow() const {
  Rect in = furnaceSlot(0), out = furnaceSlot(2);
  float h_ = std::round(slot * 0.3f);
  float x = in.right() + gap_ * 2;
  float point = h_ * 0.8f; // the arrowhead drawn past the track's end
  return {x, std::round(in.center().y - h_ * 0.5f), std::max(0.0f, out.x - gap_ * 2 - point - x), h_};
}

Rect HudLayout::furnaceFlame() const {
  Rect fuel = furnaceSlot(1);
  float s = std::round(slot * 0.6f);
  return {fuel.right() + gap_ * 2, std::round(fuel.center().y - s * 0.5f), s, s};
}

std::optional<int> HudLayout::hitTestFurnace(float px, float py) const {
  for (int i = 0; i < 3; ++i)
    if (furnaceSlot(i).contains(px, py)) return i;
  return std::nullopt;
}
