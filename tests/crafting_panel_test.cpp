#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "ui/crafting_panel.hpp"
#include "ui/hud_layout.hpp"

namespace {
ItemId block(BlockType b) { return itemForBlock(b); }
} // namespace

TEST_CASE("known recipes come first, then one ??? row per hidden recipe") {
  SeenItems seen;
  see(seen, block(BlockType::OAK_LOG));
  Inventory inv;
  inv.set(0, {block(BlockType::OAK_LOG), 3});
  auto rows = craftingRows(seen, false, inv, false);
  REQUIRE(rows.size() == recipes().size());
  CHECK(rows[0].known);
  CHECK(rows[0].recipe == 0); // oak planks
  CHECK(rows[0].affordable == 3);
  CHECK(rows[0].canCraft());
  CHECK_FALSE(rows[1].known);
  CHECK(hiddenCount(rows) == static_cast<int>(recipes().size()) - 1);
}

TEST_CASE("a known table recipe is dimmed with no table near") {
  SeenItems seen;
  see(seen, block(BlockType::COBBLESTONE));
  Inventory inv;
  inv.set(0, {block(BlockType::COBBLESTONE), 8});
  auto away = craftingRows(seen, false, inv, false);
  REQUIRE(away[0].recipe == 6); // stone bricks
  CHECK(away[0].needs_table);
  CHECK_FALSE(away[0].canCraft());
  auto near = craftingRows(seen, false, inv, true);
  CHECK(near[0].canCraft());
  CHECK(near[0].affordable == 2);
}

TEST_CASE("creative shows every recipe") {
  auto rows = craftingRows(SeenItems{}, true, Inventory{}, true);
  CHECK(hiddenCount(rows) == 0);
  CHECK_FALSE(rows[0].canCraft()); // nothing to make it from
}

TEST_CASE("the crafting panel sits left of the inventory, on screen, clear of the palette") {
  for (glm::vec2 size : {glm::vec2{1920, 1080}, glm::vec2{1280, 720}, glm::vec2{800, 600}}) {
    CAPTURE(size.x);
    HudLayout l(size.x, size.y);
    Rect panel = l.craftingPanel();
    Rect inv = l.screenPanel();
    CHECK(panel.x >= 0.0f);
    CHECK(panel.y >= 0.0f);
    CHECK(panel.bottom() <= size.y);
    CHECK(panel.right() <= inv.x);
    CHECK(panel.right() <= l.palettePanel(80).x);
    CHECK(l.craftingVisibleRows() >= 4);
    for (int i = 0; i < l.craftingVisibleRows(); ++i) {
      Rect r = l.craftingRow(i);
      CHECK(r.bottom() <= panel.bottom());
      CHECK(r.x >= panel.x);
      CHECK(l.hitTestCrafting(r.center().x, r.center().y) == i);
    }
    CHECK_FALSE(l.hitTestCrafting(inv.center().x, inv.center().y));
  }
}
