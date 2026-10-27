#include "item/chest.hpp"
#include <algorithm>
#include <numeric>
#include <vector>

const std::array<TreasureLoot, 11> &treasureLoot() {
  static const std::array<TreasureLoot, 11> loot = {{
      {ItemId::GOLD_INGOT, 2, 5, 20},
      {ItemId::IRON_INGOT, 3, 8, 20},
      {ItemId::COAL, 4, 10, 15},
      {ItemId::COOKED_GLOWFIN, 2, 4, 12},
      {ItemId::RUBY, 1, 3, 8},
      {ItemId::EMERALD, 1, 3, 8},
      {ItemId::DIAMOND, 1, 2, 6},
      {itemForBlock(BlockType::GLOW_CRYSTAL), 2, 4, 6},
      {ItemId::TREASURE_KEY, 1, 1, 3}, // one chest can lead to the next
      {ItemId::IRON_PICKAXE, 1, 1, 1},
      {ItemId::RUBY_SWORD, 1, 1, 1},
  }};
  return loot;
}

Chest rollTreasure(std::mt19937 &rng) {
  const auto &loot = treasureLoot();
  std::vector<int> left(loot.size());
  std::iota(left.begin(), left.end(), 0);
  int stacks = std::uniform_int_distribution<int>(kTreasureMinStacks, kTreasureMaxStacks)(rng);

  std::vector<int> slots(kChestSlots);
  std::iota(slots.begin(), slots.end(), 0);
  std::shuffle(slots.begin(), slots.end(), rng);

  Chest chest{};
  for (int n = 0; n < stacks && !left.empty(); ++n) {
    // Without repeats: each pick comes out of what's left.
    int total = 0;
    for (int i : left) total += loot[i].weight;
    int roll = std::uniform_int_distribution<int>(0, total - 1)(rng);
    size_t k = 0;
    while (roll >= loot[left[k]].weight) roll -= loot[left[k++]].weight;
    const TreasureLoot &l = loot[left[k]];
    left.erase(left.begin() + static_cast<long>(k));
    uint16_t count = static_cast<uint16_t>(std::uniform_int_distribution<int>(l.min, l.max)(rng));
    chest[slots[n]] = {l.id, count};
  }
  return chest;
}
