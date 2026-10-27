#pragma once

#include "inventory/inventory.hpp"
#include "item/recipes.hpp"
#include <vector>

// What the crafting panel shows. Pure: built from what the client knows (the
// items it's been told it has seen, its inventory, whether a table is near)
// with the same rules the Game checks, so a bright row is one the Game will
// accept.
struct CraftRow {
  uint16_t recipe = 0;    // index into recipes()
  bool known = false;     // false: a "???" row
  int affordable = 0;     // times the ingredients stretch
  bool needs_table = false; // and no table is in reach
  bool canCraft() const { return known && affordable > 0 && !needs_table; }
};

// Known recipes first (in recipe order), then one "???" row per hidden one.
std::vector<CraftRow> craftingRows(const SeenItems &seen, bool know_all,
                                   const Inventory &inventory, bool table_near);
int hiddenCount(const std::vector<CraftRow> &rows);
