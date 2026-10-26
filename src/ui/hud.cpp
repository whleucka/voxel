#include "ui/hud.hpp"
#include "block/block_data.hpp"
#include "imgui/imgui.h"
#include "ui/item_icon_geometry.hpp"
#include <algorithm>
#include <iostream>
#include <stb_image.h>
#include <string>

namespace {

GLuint loadTexture(const std::string &path) {
  int w = 0, h = 0, channels = 0;
  stbi_set_flip_vertically_on_load(false);
  unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 4);
  if (!data) {
    std::cerr << "[hud] failed to load " << path << "\n";
    return 0;
  }
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  // sRGB like the block atlas, so the framebuffer's sRGB encode gives back
  // the colours as painted.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, w, h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  stbi_image_free(data);
  return tex;
}

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
  block_tex = loadTexture(block_atlas);
  item_tex = loadTexture(item_atlas);
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
