#include "ui/hud.hpp"
#include "block/block_data.hpp"
#include "ui/logo.hpp"
#include "imgui/imgui.h"
#include "render/texture_util.hpp"
#include "ui/item_icon_geometry.hpp"
#include "vitals/vitals.hpp"
#include <algorithm>
#include <string>

namespace {

ImVec2 iv(glm::vec2 v) { return {v.x, v.y}; }

ImU32 rgba(int r, int g, int b, float a) {
  return IM_COL32(r, g, b, static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255));
}

// Text with a 1px drop shadow so it reads over any icon.
void shadowText(ImDrawList *dl, float size, ImVec2 pos, ImU32 col, float alpha,
                const char *text) {
  dl->AddText(nullptr, size, ImVec2(pos.x + 1, pos.y + 1), rgba(0, 0, 0, alpha * 0.8f), text);
  dl->AddText(nullptr, size, pos, col, text);
}

} // namespace

Hud::~Hud() { shutdown(); }

void Hud::shutdown() {
  if (block_tex) glDeleteTextures(1, &block_tex);
  if (item_tex) glDeleteTextures(1, &item_tex);
  block_tex = item_tex = 0;
}

bool Hud::init(const std::string &block_atlas, const std::string &item_atlas) {
  block_tex = loadSrgbTexture(block_atlas);
  item_tex = loadSrgbTexture(item_atlas);
  return block_tex && item_tex;
}

// ─── Pieces ────────────────────────────────────────────────────────────

void Hud::drawIcon(ImDrawList *dl, const Rect &r, ItemId id, float alpha) const {
  const ItemDef &def = itemDef(id);
  float pad = std::round(r.w * 0.14f);

  if (def.places) {
    if (!block_tex) return;
    auto tex = block_data.find(*def.places);
    if (tex == block_data.end()) return;
    UvRect top = tileUv(tex->second.top, kBlockAtlasTiles);
    UvRect side = tileUv(tex->second.side, kBlockAtlasTiles);
    CubeQuads c = cubeQuads(r, pad);
    ImTextureID t = static_cast<ImTextureID>(block_tex);
    auto face = [&](const Quad &q, const UvRect &uv, int shade) {
      dl->AddImageQuad(t, iv(q.p[0]), iv(q.p[1]), iv(q.p[2]), iv(q.p[3]),
                       ImVec2(uv.uv0.x, uv.uv0.y), ImVec2(uv.uv1.x, uv.uv0.y),
                       ImVec2(uv.uv1.x, uv.uv1.y), ImVec2(uv.uv0.x, uv.uv1.y),
                       rgba(shade, shade, shade, alpha));
    };
    face(c.left, side, 150);  // darker left, lighter right, full-bright top
    face(c.right, side, 205);
    face(c.top, top, 255);
    return;
  }

  if (!item_tex) return;
  UvRect uv = iconUv(def.icon);
  dl->AddImage(static_cast<ImTextureID>(item_tex), ImVec2(r.x + pad, r.y + pad),
               ImVec2(r.right() - pad, r.bottom() - pad), iv(uv.uv0), iv(uv.uv1),
               rgba(255, 255, 255, alpha));
}

void Hud::drawCount(ImDrawList *dl, const Rect &r, uint16_t count, float alpha) const {
  if (count <= 1) return;
  std::string text = std::to_string(count);
  float size = std::round(r.h * 0.32f);
  ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
  ImVec2 pos(r.right() - ext.x - r.w * 0.06f, r.bottom() - ext.y - r.h * 0.02f);
  shadowText(dl, size, pos, rgba(255, 255, 255, alpha), alpha, text.c_str());
}

void Hud::drawSlot(ImDrawList *dl, const Rect &r, const ItemStack &s,
                   bool highlighted, float alpha, int number_label) const {
  ImVec2 a(r.x, r.y), b(r.right(), r.bottom());
  dl->AddRectFilled(a, b, rgba(0, 0, 0, 0.45f));
  if (highlighted)
    dl->AddRect(a, b, rgba(255, 235, 140, 1.0f), 0.0f, 0, std::max(2.0f, r.w * 0.05f));
  else
    dl->AddRect(a, b, rgba(255, 255, 255, 0.25f));

  if (number_label >= 0) {
    std::string n = std::to_string(number_label);
    shadowText(dl, std::round(r.h * 0.22f), ImVec2(r.x + 3, r.y + 1),
               rgba(255, 255, 255, 0.45f), 0.6f, n.c_str());
  }
  if (!s.empty()) {
    drawIcon(dl, r, s.id, alpha);
    drawCount(dl, r, s.count, alpha);
  }
}

// ─── Hotbar ────────────────────────────────────────────────────────────

void Hud::drawHotbar(const HudLayout &layout, const Inventory::Slots &inv,
                     int selected, double now) {
  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  for (int i = 0; i < kHotbarSlots; ++i)
    drawSlot(dl, layout.hotbarSlot(i), inv[i], i == selected, 1.0f,
             (i + 1) % kHotbarSlots);

  // Name popup: restart whenever the selected slot or what's in it changes.
  ItemId held = inv[selected].empty() ? ItemId::NONE : inv[selected].id;
  if (selected != popup_slot || held != popup_item) {
    popup_slot = selected;
    popup_item = held;
    popup_since = now;
  }
  double age = now - popup_since;
  if (held == ItemId::NONE || age > kNameShowSeconds) return;

  float alpha = age < kNameShowSeconds - kNameFadeSeconds
                    ? 1.0f
                    : static_cast<float>((kNameShowSeconds - age) / kNameFadeSeconds);
  Rect area = layout.nameArea();
  std::string name(itemDef(held).name);
  float size = area.h;
  ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, name.c_str());
  shadowText(dl, size, ImVec2(area.center().x - ext.x * 0.5f, area.y),
             rgba(255, 255, 255, alpha), alpha, name.c_str());
}

// ─── Inventory screen ──────────────────────────────────────────────────

void Hud::drawInventoryScreen(const HudLayout &layout, const Inventory::Slots &inv,
                              const InventoryScreen &screen, glm::vec2 mouse) {
  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  ImVec2 display = ImGui::GetIO().DisplaySize;
  dl->AddRectFilled(ImVec2(0, 0), display, rgba(0, 0, 0, 0.45f));

  Rect panel = layout.screenPanel();
  dl->AddRectFilled(ImVec2(panel.x, panel.y), ImVec2(panel.right(), panel.bottom()),
                    rgba(30, 30, 36, 0.92f), layout.slotSize() * 0.12f);

  std::optional<int> held = screen.held();
  std::optional<int> hover = layout.hitTestScreen(mouse.x, mouse.y);

  // A stack held from a furnace slot is drawn by drawFurnace.
  if (held && !Inventory::validSlot(*held)) held.reset();
  for (int i = 0; i < kInventorySlots; ++i) {
    bool is_held = held && *held == i;
    drawSlot(dl, layout.screenSlot(i), inv[i], hover && *hover == i,
             is_held ? 0.35f : 1.0f, i < kHotbarSlots ? (i + 1) % kHotbarSlots : -1);
  }

  // Draw on the foreground list so the carried stack and tooltip sit above
  // everything, including the panel.
  ImDrawList *fg = ImGui::GetForegroundDrawList();
  if (held && !inv[*held].empty()) {
    float s = layout.slotSize();
    Rect carried{mouse.x - s * 0.5f, mouse.y - s * 0.5f, s, s};
    drawIcon(fg, carried, inv[*held].id, 1.0f);
    drawCount(fg, carried, inv[*held].count, 1.0f);
  } else if (hover && !inv[*hover].empty()) {
    std::string name(itemDef(inv[*hover].id).name);
    float size = std::round(layout.slotSize() * 0.3f);
    ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, name.c_str());
    ImVec2 pos(mouse.x + 14, mouse.y - ext.y - 6);
    fg->AddRectFilled(ImVec2(pos.x - 5, pos.y - 3),
                      ImVec2(pos.x + ext.x + 5, pos.y + ext.y + 3),
                      rgba(15, 15, 20, 0.92f), 3.0f);
    fg->AddText(nullptr, size, pos, rgba(255, 255, 255, 1.0f), name.c_str());
  }
}

void Hud::drawPalette(const HudLayout &layout, const std::vector<ItemId> &items,
                      glm::vec2 mouse) {
  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  int count = static_cast<int>(items.size());
  Rect panel = layout.palettePanel(count);
  dl->AddRectFilled(ImVec2(panel.x, panel.y), ImVec2(panel.right(), panel.bottom()),
                    rgba(30, 30, 36, 0.92f), layout.slotSize() * 0.12f);
  std::optional<int> hover = layout.hitTestPalette(mouse.x, mouse.y, count);
  for (int i = 0; i < count; ++i)
    drawSlot(dl, layout.paletteSlot(i, count), {items[i], 1}, hover && *hover == i, 1.0f, -1);

  if (!hover) return;
  std::string name(itemDef(items[*hover]).name);
  float size = std::round(layout.slotSize() * 0.3f);
  ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, name.c_str());
  ImVec2 pos(mouse.x - ext.x - 14, mouse.y - ext.y - 6); // left: the panel is at the edge
  ImDrawList *fg = ImGui::GetForegroundDrawList();
  fg->AddRectFilled(ImVec2(pos.x - 5, pos.y - 3), ImVec2(pos.x + ext.x + 5, pos.y + ext.y + 3),
                    rgba(15, 15, 20, 0.92f), 3.0f);
  fg->AddText(nullptr, size, pos, rgba(255, 255, 255, 1.0f), name.c_str());
}

// ─── Furnace ───────────────────────────────────────────────────────────

void Hud::drawFurnace(const HudLayout &layout, const ContainerView &view,
                      const InventoryScreen &screen, glm::vec2 mouse) {
  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  Rect panel = layout.furnacePanel();
  if (panel.w <= 0) return;
  dl->AddRectFilled(ImVec2(panel.x, panel.y), ImVec2(panel.right(), panel.bottom()),
                    rgba(30, 30, 36, 0.92f), layout.slotSize() * 0.12f);
  Rect title = layout.furnaceTitle();
  shadowText(dl, title.h, ImVec2(title.x, title.y), rgba(255, 255, 255, 1.0f), 1.0f, "Furnace");

  const ItemStack stacks[3] = {view.input, view.fuel, view.output};
  std::optional<int> held = screen.held();
  std::optional<int> hover = layout.hitTestFurnace(mouse.x, mouse.y);
  for (int i = 0; i < 3; ++i) {
    bool is_held = held && *held == kFurnaceInput + i;
    drawSlot(dl, layout.furnaceSlot(i), stacks[i], hover && *hover == i, is_held ? 0.35f : 1.0f, -1);
  }

  // Cooking: an arrow track filling left to right.
  Rect arrow = layout.furnaceArrow();
  dl->AddRectFilled(ImVec2(arrow.x, arrow.y), ImVec2(arrow.right(), arrow.bottom()),
                    rgba(0, 0, 0, 0.45f), 2.0f);
  float done = arrow.w * view.cook / 255.0f;
  if (done > 0)
    dl->AddRectFilled(ImVec2(arrow.x, arrow.y), ImVec2(arrow.x + done, arrow.bottom()),
                      rgba(240, 240, 245, 0.95f), 2.0f);
  ImVec2 point(arrow.right() + arrow.h * 0.8f, arrow.center().y);
  dl->AddTriangleFilled(ImVec2(arrow.right(), arrow.y - arrow.h * 0.5f), point,
                        ImVec2(arrow.right(), arrow.bottom() + arrow.h * 0.5f),
                        view.cook == 255 ? rgba(240, 240, 245, 0.95f) : rgba(0, 0, 0, 0.45f));

  // Fuel left: a flame that shrinks from the top as the burn runs down.
  Rect flame = layout.furnaceFlame();
  dl->AddRectFilled(ImVec2(flame.x, flame.y), ImVec2(flame.right(), flame.bottom()),
                    rgba(0, 0, 0, 0.45f), 3.0f);
  float lit = flame.h * view.burn / 255.0f;
  if (lit > 0) {
    float top = flame.bottom() - lit;
    dl->AddRectFilledMultiColor(ImVec2(flame.x + 2, top), ImVec2(flame.right() - 2, flame.bottom() - 2),
                                rgba(255, 220, 90, 1.0f), rgba(255, 220, 90, 1.0f),
                                rgba(230, 80, 20, 1.0f), rgba(230, 80, 20, 1.0f));
  }

  ImDrawList *fg = ImGui::GetForegroundDrawList();
  if (held && *held >= kFurnaceInput && *held <= kFurnaceOutput) {
    const ItemStack &carried = stacks[*held - kFurnaceInput];
    if (!carried.empty()) {
      float s = layout.slotSize();
      Rect r{mouse.x - s * 0.5f, mouse.y - s * 0.5f, s, s};
      drawIcon(fg, r, carried.id, 1.0f);
      drawCount(fg, r, carried.count, 1.0f);
    }
    return;
  }
  if (!hover) return;
  static const char *const kEmpty[3] = {"Something to smelt", "Fuel: coal, charcoal or wood",
                                        "What comes out"};
  std::string tip = stacks[*hover].empty() ? kEmpty[*hover] : std::string(itemDef(stacks[*hover].id).name);
  float size = std::round(layout.slotSize() * 0.3f);
  ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, tip.c_str());
  ImVec2 pos(mouse.x + 14, mouse.y - ext.y - 6);
  fg->AddRectFilled(ImVec2(pos.x - 5, pos.y - 3), ImVec2(pos.x + ext.x + 5, pos.y + ext.y + 3),
                    rgba(15, 15, 20, 0.92f), 3.0f);
  fg->AddText(nullptr, size, pos, rgba(255, 255, 255, 1.0f), tip.c_str());
}

// ─── Crafting ──────────────────────────────────────────────────────────

void Hud::drawCrafting(const HudLayout &layout, const std::vector<CraftRow> &rows, int first,
                       const Inventory &inventory, glm::vec2 mouse) {
  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  Rect panel = layout.craftingPanel();
  if (panel.w <= 0) return;
  dl->AddRectFilled(ImVec2(panel.x, panel.y), ImVec2(panel.right(), panel.bottom()),
                    rgba(30, 30, 36, 0.92f), layout.slotSize() * 0.12f);

  Rect title = layout.craftingTitle();
  shadowText(dl, title.h, ImVec2(title.x, title.y), rgba(255, 255, 255, 1.0f), 1.0f, "Crafting");
  if (int hidden = hiddenCount(rows)) {
    std::string left = std::to_string(hidden) + " to discover";
    float size = std::round(title.h * 0.7f);
    ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, left.c_str());
    dl->AddText(nullptr, size, ImVec2(title.right() - ext.x, title.bottom() - ext.y),
                rgba(200, 200, 210, 0.8f), left.c_str());
  }

  std::optional<int> hover = layout.hitTestCrafting(mouse.x, mouse.y);
  const int visible = layout.craftingVisibleRows();
  for (int i = 0; i < visible && first + i < static_cast<int>(rows.size()); ++i) {
    const CraftRow &row = rows[first + i];
    Rect r = layout.craftingRow(i);
    bool lit = row.canCraft();
    dl->AddRectFilled(ImVec2(r.x, r.y), ImVec2(r.right(), r.bottom()),
                      lit ? rgba(64, 64, 76, 0.95f) : rgba(42, 42, 50, 0.8f), 3.0f);
    if (lit && hover && *hover == i)
      dl->AddRect(ImVec2(r.x, r.y), ImVec2(r.right(), r.bottom()), rgba(255, 235, 140, 1.0f),
                  3.0f, 0, std::max(2.0f, r.h * 0.04f));

    Rect icon{r.x + 2, r.y + 2, r.h - 4, r.h - 4};
    float text_x = icon.right() + layout.gap() * 2;
    float name_size = std::round(r.h * 0.3f);
    if (!row.known) {
      dl->AddRectFilled(ImVec2(icon.x, icon.y), ImVec2(icon.right(), icon.bottom()),
                        rgba(0, 0, 0, 0.45f));
      shadowText(dl, std::round(icon.h * 0.6f), ImVec2(icon.center().x - icon.h * 0.15f, icon.y + icon.h * 0.15f),
                 rgba(160, 160, 170, 1.0f), 1.0f, "?");
      shadowText(dl, name_size, ImVec2(text_x, r.y + r.h * 0.33f), rgba(150, 150, 160, 1.0f),
                 1.0f, "???");
      continue;
    }

    const Recipe &recipe = recipes()[row.recipe];
    float alpha = lit ? 1.0f : 0.55f;
    drawIcon(dl, icon, recipe.makes.id, alpha);
    drawCount(dl, icon, recipe.makes.count, alpha);

    std::string name(itemDef(recipe.makes.id).name);
    shadowText(dl, name_size, ImVec2(text_x, r.y + r.h * 0.08f), rgba(255, 255, 255, alpha), alpha,
               name.c_str());
    if (row.needs_table) {
      const char *note = "needs a table";
      float size = std::round(name_size * 0.8f);
      ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, note);
      dl->AddText(nullptr, size, ImVec2(r.right() - ext.x - 4, r.y + r.h * 0.1f),
                  rgba(240, 190, 90, 1.0f), note);
    }

    // Ingredients: an icon and have/need each, red when short.
    float ing = std::round(r.h * 0.42f);
    float x = text_x;
    float y = r.bottom() - ing - r.h * 0.06f;
    for (const Ingredient &in : recipe.needs) {
      ItemId shown = in.any_of.front();
      for (ItemId id : in.any_of)
        for (const ItemStack &s : inventory.slots())
          if (s.id == id) shown = id;
      Rect small{x, y, ing, ing};
      drawIcon(dl, small, shown, alpha);
      int have = countOf(inventory, in);
      std::string text = std::to_string(std::min(have, 999)) + "/" + std::to_string(in.count);
      float size = std::round(ing * 0.6f);
      ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
      ImU32 col = have >= in.count ? rgba(220, 220, 225, alpha) : rgba(255, 110, 100, 1.0f);
      dl->AddText(nullptr, size, ImVec2(small.right() + 2, y + (ing - ext.y) * 0.5f), col,
                  text.c_str());
      x = small.right() + ext.x + layout.gap() * 3;
    }
  }

  // More above or below: a hint that the list scrolls.
  float hint = std::round(layout.slotSize() * 0.25f);
  if (first > 0)
    dl->AddText(nullptr, hint, ImVec2(title.center().x, title.bottom() - hint),
                rgba(200, 200, 210, 0.8f), "^");
  if (first + visible < static_cast<int>(rows.size()))
    dl->AddText(nullptr, hint * 1.2f, ImVec2(title.center().x, panel.bottom() - hint * 1.4f),
                rgba(200, 200, 210, 0.8f), "more...");

  // Tooltip: what a dimmed row is waiting for.
  if (!hover || first + *hover >= static_cast<int>(rows.size())) return;
  const CraftRow &row = rows[first + *hover];
  std::string tip = !row.known      ? "Find all of its ingredients to discover it"
                    : row.needs_table ? "Stand near a Crafting Table"
                    : row.affordable == 0 ? "Missing ingredients"
                                          : "Click: make one  -  Shift-click: make all";
  float size = std::round(layout.slotSize() * 0.3f);
  ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, tip.c_str());
  ImVec2 pos(mouse.x + 14, mouse.y - ext.y - 6);
  ImDrawList *fg = ImGui::GetForegroundDrawList();
  fg->AddRectFilled(ImVec2(pos.x - 5, pos.y - 3), ImVec2(pos.x + ext.x + 5, pos.y + ext.y + 3),
                    rgba(15, 15, 20, 0.92f), 3.0f);
  fg->AddText(nullptr, size, pos, rgba(255, 255, 255, 1.0f), tip.c_str());
}

void Hud::drawNotice(const HudLayout &layout, double now) {
  double age = now - notice_since;
  if (notice_text.empty() || age > kNoticeSeconds) return;
  float alpha = age < kNoticeSeconds - 0.5 ? 1.0f : static_cast<float>((kNoticeSeconds - age) / 0.5);
  Rect area = layout.noticeArea();
  float size = area.h;
  ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, notice_text.c_str());
  shadowText(ImGui::GetForegroundDrawList(), size,
             ImVec2(area.center().x - ext.x * 0.5f, area.y), rgba(255, 225, 120, alpha), alpha,
             notice_text.c_str());
}

// ─── Title logo ────────────────────────────────────────────────────────

void Hud::drawLogo(float display_w, float display_h) {
  ImDrawList *dl = ImGui::GetForegroundDrawList();
  LogoLayout l = logoLayout(kGameTitle, display_w, display_h);
  std::vector<LogoCell> cells = logoCells(kGameTitle);
  float s = l.cell;
  auto cellRect = [&](const LogoCell &c, float dx, float dy) {
    ImVec2 a(l.x + c.col * s + dx, l.y + c.row * s + dy);
    return std::pair{a, ImVec2(a.x + s, a.y + s)};
  };

  // A soft drop shadow first, so the blocks stand off the scene.
  float off = std::max(2.0f, std::round(s * 0.28f));
  for (const LogoCell &c : cells) {
    auto [a, b] = cellRect(c, off, off);
    dl->AddRectFilled(a, b, rgba(0, 0, 0, 0.55f));
  }

  for (const LogoCell &c : cells) {
    auto [a, b] = cellRect(c, 0, 0);
    auto tex = block_data.find(c.block);
    if (block_tex && tex != block_data.end()) {
      UvRect uv = tileUv(tex->second.side, kBlockAtlasTiles);
      dl->AddImage(static_cast<ImTextureID>(block_tex), a, b, iv(uv.uv0), iv(uv.uv1));
    } else { // no atlas: plain colours
      ImU32 col = c.block == BlockType::GRASS   ? rgba(95, 160, 60, 1.0f)
                  : c.block == BlockType::STONE ? rgba(125, 125, 130, 1.0f)
                                                : rgba(90, 200, 220, 1.0f);
      dl->AddRectFilled(a, b, col);
    }
    dl->AddRect(a, b, rgba(0, 0, 0, 0.35f)); // block edges
  }
}

// ─── Vitals ────────────────────────────────────────────────────────────

namespace {

constexpr int kIcons = 10;

// Simple vector icons centred on c with size s; no art needed. `fill` is the
// colour, `outline` is drawn slightly larger behind it.
void heart(ImDrawList *dl, ImVec2 c, float s, ImU32 col) {
  float r = s * 0.27f;
  dl->AddCircleFilled(ImVec2(c.x - s * 0.22f, c.y - s * 0.12f), r, col);
  dl->AddCircleFilled(ImVec2(c.x + s * 0.22f, c.y - s * 0.12f), r, col);
  dl->AddTriangleFilled(ImVec2(c.x - s * 0.48f, c.y - s * 0.04f),
                        ImVec2(c.x + s * 0.48f, c.y - s * 0.04f),
                        ImVec2(c.x, c.y + s * 0.46f), col);
}

void bolt(ImDrawList *dl, ImVec2 c, float s, ImU32 col) {
  const ImVec2 pts[] = {
      {c.x + s * 0.12f, c.y - s * 0.50f}, {c.x - s * 0.30f, c.y + s * 0.06f},
      {c.x - s * 0.02f, c.y + s * 0.06f}, {c.x - s * 0.14f, c.y + s * 0.50f},
      {c.x + s * 0.30f, c.y - s * 0.08f}, {c.x + s * 0.02f, c.y - s * 0.08f},
  };
  dl->AddConcavePolyFilled(pts, 6, col);
}

void bubble(ImDrawList *dl, ImVec2 c, float s, ImU32 col) {
  dl->AddCircleFilled(c, s * 0.38f, col);
  dl->AddCircleFilled(ImVec2(c.x - s * 0.12f, c.y - s * 0.12f), s * 0.09f,
                      IM_COL32(255, 255, 255, 220));
}

} // namespace

void Hud::drawIconRow(ImDrawList *dl, const Rect &strip, Icon icon, int value,
                      bool right_aligned) const {
  float s = strip.h;
  float step = s * 1.22f; // room for the outlines between icons
  auto shape = [&](ImVec2 c, float size, ImU32 col) {
    switch (icon) {
      case Icon::Heart:  heart(dl, c, size, col); break;
      case Icon::Bolt:   bolt(dl, c, size, col); break;
      case Icon::Bubble: bubble(dl, c, size, col); break;
    }
  };
  ImU32 full = icon == Icon::Heart  ? IM_COL32(220, 30, 40, 255)
             : icon == Icon::Bolt   ? IM_COL32(250, 200, 40, 255)
                                    : IM_COL32(90, 170, 255, 255);

  for (int i = 0; i < kIcons; ++i) {
    // Hearts and bubbles fill from the left; bolts mirror from the right.
    int slot = right_aligned ? kIcons - 1 - i : i;
    float cx = right_aligned ? strip.right() - s * 0.5f - slot * step
                             : strip.x + s * 0.5f + slot * step;
    ImVec2 c(cx, strip.y + s * 0.5f);
    int points = std::clamp(value - i * 2, 0, 2);

    if (icon == Icon::Bubble) {
      // Bubbles just disappear as breath runs out.
      if (points > 0) shape(c, s * (points == 2 ? 1.0f : 0.7f), full);
      continue;
    }
    shape(c, s * 1.18f, IM_COL32(0, 0, 0, 200));    // outline
    shape(c, s, IM_COL32(60, 60, 60, 230));         // empty
    if (points == 2) {
      shape(c, s, full);
    } else if (points == 1) {
      // Half icon: the half nearest the row's start.
      ImVec2 a = right_aligned ? ImVec2(c.x, c.y - s) : ImVec2(c.x - s, c.y - s);
      ImVec2 b = right_aligned ? ImVec2(c.x + s, c.y + s) : ImVec2(c.x, c.y + s);
      dl->PushClipRect(a, b, true);
      shape(c, s, full);
      dl->PopClipRect();
    }
  }
}

void Hud::drawVitals(const HudLayout &layout, const VitalsChanged &v, double now) {
  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  Rect strip = layout.barStrip();
  drawIconRow(dl, strip, Icon::Heart, v.health, false);
  drawIconRow(dl, strip, Icon::Bolt, v.stamina, true);
  if (v.breath < Vitals::kMax)
    drawIconRow(dl, layout.breathStrip(), Icon::Bubble, v.breath, false);

  // Red edges fading out after damage.
  constexpr double kFlash = 0.4;
  double age = now - damage_since;
  if (age >= 0 && age < kFlash) {
    float a = static_cast<float>(1.0 - age / kFlash) * 0.55f;
    ImVec2 d = ImGui::GetIO().DisplaySize;
    float e = std::min(d.x, d.y) * 0.12f;
    ImU32 red = rgba(200, 0, 0, a), clear = rgba(200, 0, 0, 0.0f);
    ImDrawList *fg = ImGui::GetForegroundDrawList();
    fg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(d.x, e), red, red, clear, clear);
    fg->AddRectFilledMultiColor(ImVec2(0, d.y - e), d, clear, clear, red, red);
    fg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(e, d.y), red, clear, clear, red);
    fg->AddRectFilledMultiColor(ImVec2(d.x - e, 0), d, clear, red, red, clear);
  }
}

void Hud::drawDeathScreen() {
  ImDrawList *fg = ImGui::GetForegroundDrawList();
  ImVec2 d = ImGui::GetIO().DisplaySize;
  fg->AddRectFilled(ImVec2(0, 0), d, rgba(90, 0, 0, 0.55f));

  const char *title = "You died";
  float size = std::round(d.y * 0.08f);
  ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, title);
  shadowText(fg, size, ImVec2((d.x - ext.x) * 0.5f, d.y * 0.38f),
             rgba(255, 255, 255, 1.0f), 1.0f, title);

  const char *sub = "Respawning...";
  float sub_size = std::round(size * 0.35f);
  ImVec2 sub_ext = ImGui::GetFont()->CalcTextSizeA(sub_size, FLT_MAX, 0.0f, sub);
  shadowText(fg, sub_size, ImVec2((d.x - sub_ext.x) * 0.5f, d.y * 0.38f + size * 1.3f),
             rgba(230, 230, 230, 1.0f), 1.0f, sub);
}
