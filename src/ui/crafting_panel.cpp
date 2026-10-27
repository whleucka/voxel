#include "ui/crafting_panel.hpp"
#include <algorithm>

std::vector<CraftRow> craftingRows(const SeenItems &seen, bool know_all,
                                   const Inventory &inventory, bool table_near) {
  std::vector<CraftRow> known, hidden;
  const auto &all = recipes();
  for (size_t i = 0; i < all.size(); ++i) {
    CraftRow row;
    row.recipe = static_cast<uint16_t>(i);
    row.known = know_all || knows(all[i], seen);
    if (!row.known) {
      hidden.push_back(row);
      continue;
    }
    row.affordable = timesAffordable(all[i], inventory);
    row.needs_table = all[i].needs_table && !table_near;
    known.push_back(row);
  }
  known.insert(known.end(), hidden.begin(), hidden.end());
  return known;
}

int hiddenCount(const std::vector<CraftRow> &rows) {
  return static_cast<int>(
      std::count_if(rows.begin(), rows.end(), [](const CraftRow &r) { return !r.known; }));
}
