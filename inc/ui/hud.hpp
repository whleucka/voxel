#pragma once

#include "inventory/inventory.hpp"
#include "ui/hud_layout.hpp"
#include "ui/inventory_screen.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

struct ImDrawList;

// Draws the hotbar and the inventory screen with ImGui draw lists. Owns its own
// copies of the block and item atlases (sRGB, nearest filtering, matching the
// world's colours). All geometry comes from HudLayout / item_icon_geometry.
class Hud {
public:
  Hud() = default;
  ~Hud();
  Hud(const Hud &) = delete;
  Hud &operator=(const Hud &) = delete;

  // Needs a current GL context. Returns false if an atlas fails to load; the
  // HUD then draws slots without icons.
  bool init(const std::string &block_atlas, const std::string &item_atlas);
  // Frees the textures; call while the GL context is still current.
  void shutdown();

  // In-game hotbar plus the held-item name popup. `now` is in seconds.
  void drawHotbar(const HudLayout &layout, const Inventory::Slots &inv,
                  int selected, double now);

  // The inventory screen, the stack being carried, and the hover tooltip.
  void drawInventoryScreen(const HudLayout &layout, const Inventory::Slots &inv,
                           const InventoryScreen &screen, glm::vec2 mouse);

private:
  static constexpr double kNameShowSeconds = 1.5;
  static constexpr double kNameFadeSeconds = 0.5;

  GLuint block_tex = 0;
  GLuint item_tex = 0;

  // What the name popup last showed, to notice selection/item changes.
  int popup_slot = -1;
  ItemId popup_item = ItemId::NONE;
  double popup_since = -100.0;

  void drawSlot(ImDrawList *dl, const Rect &r, const ItemStack &s,
                bool highlighted, float alpha, int number_label) const;
  void drawIcon(ImDrawList *dl, const Rect &r, ItemId id, float alpha) const;
  void drawCount(ImDrawList *dl, const Rect &r, uint16_t count, float alpha) const;
};
