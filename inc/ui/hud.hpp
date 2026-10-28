#pragma once

#include "inventory/inventory.hpp"
#include "sim/protocol.hpp"
#include "ui/crafting_panel.hpp"
#include "ui/hud_layout.hpp"
#include "ui/inventory_screen.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

struct ImDrawList;

// Draws the hotbar and the inventory screen with ImGui draw lists. Owns its own
// copies of the block and item atlases (sRGB, nearest filtering, matching the
// world's colours). All geometry comes from HudLayout / item_icon_geometry.
// The popup for a catch: "Caught a Sunfin!" (raw fish are named by kind).
std::string caughtMessage(ItemId item);

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

  // Hearts, stamina bolts and (when below full) breath bubbles, plus the
  // damage flash. Drawn with the hotbar during play.
  void drawVitals(const HudLayout &layout, const VitalsChanged &v, double now);
  // Full-screen "You died" while waiting to respawn.
  void drawDeathScreen();
  // In bed: the screen darkens over `fade` (0..1), with how many of the
  // living players are in bed when there's more than one.
  void drawSleep(float fade, int sleepers, int living);

  // Start the red edge flash shown after taking damage.
  void flashDamage(double now) { damage_since = now; }

  // The inventory screen, the stack being carried, and the hover tooltip.
  void drawInventoryScreen(const HudLayout &layout, const Inventory::Slots &inv,
                           const InventoryScreen &screen, glm::vec2 mouse);
  // The game's title as a wall of blocks (title screen).
  void drawLogo(float display_w, float display_h);

  // The creative palette beside it: one of every item, with a tooltip.
  void drawPalette(const HudLayout &layout, const std::vector<ItemId> &items,
                   glm::vec2 mouse);

  // An open furnace, in the crafting panel's place: its slots, the cooking
  // arrow and the flame, plus the stack carried from one of its slots.
  void drawFurnace(const HudLayout &layout, const ContainerView &view,
                   const InventoryScreen &screen, glm::vec2 mouse);

  // An open chest, in the crafting panel's place, plus the stack carried
  // from one of its slots.
  void drawChest(const HudLayout &layout, const ContainerView &view,
                 const InventoryScreen &screen, glm::vec2 mouse);

  // The crafting panel left of it, from row `first` down.
  void drawCrafting(const HudLayout &layout, const std::vector<CraftRow> &rows, int first,
                    const Inventory &inventory, glm::vec2 mouse);

  // A short message above the hotbar ("New recipe: ..."), shown for a few
  // seconds from `now`; drawNotice draws it while it lasts.
  void notice(std::string text, double now) {
    notice_text = std::move(text);
    notice_since = now;
  }
  void drawNotice(const HudLayout &layout, double now);
  // While the local player's bobber is biting: a pulsing prompt to reel in.
  void drawBitePrompt(const HudLayout &layout, double now);

  // How far the block being mined has got, 0..1, as a ring round the crosshair.
  void drawMiningRing(float progress);

private:
  static constexpr double kNameShowSeconds = 1.5;
  static constexpr double kNameFadeSeconds = 0.5;

  GLuint block_tex = 0;
  GLuint item_tex = 0;

  // What the name popup last showed, to notice selection/item changes.
  int popup_slot = -1;
  ItemId popup_item = ItemId::NONE;
  double popup_since = -100.0;
  double damage_since = -100.0;
  std::string notice_text;
  double notice_since = -100.0;
  static constexpr double kNoticeSeconds = 3.0;

  void drawSlot(ImDrawList *dl, const Rect &r, const ItemStack &s,
                bool highlighted, float alpha, int number_label) const;
  void drawIcon(ImDrawList *dl, const Rect &r, ItemId id, float alpha) const;
  void drawCount(ImDrawList *dl, const Rect &r, uint16_t count, float alpha) const;
  // A row of 10 icons for a 0..20 value (2 points per icon, halves allowed).
  enum class Icon { Heart, Bolt, Bubble };
  void drawIconRow(ImDrawList *dl, const Rect &strip, Icon icon, int value,
                   bool right_aligned) const;
};
