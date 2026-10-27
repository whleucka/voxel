#include "item/smelting.hpp"
#include "block/block_type.hpp"
#include <algorithm>

namespace {

ItemId block(BlockType b) { return itemForBlock(b); }

bool isLog(ItemId id) {
  return id == block(BlockType::OAK_LOG) || id == block(BlockType::PINE_LOG) ||
         id == block(BlockType::PALM_LOG) || id == block(BlockType::CHERRY_LOG);
}

bool isPlanks(ItemId id) {
  return id == block(BlockType::OAK_PLANKS) || id == block(BlockType::PINE_PLANKS) ||
         id == block(BlockType::PALM_PLANKS) || id == block(BlockType::CHERRY_PLANKS);
}

// Room in the output for what the input makes.
bool canCook(const Furnace &f) {
  if (f.input.empty()) return false;
  auto result = smeltResult(f.input.id);
  if (!result) return false;
  if (f.output.empty()) return true;
  return f.output.id == *result && f.output.space() > 0;
}

} // namespace

std::optional<ItemId> smeltResult(ItemId in) {
  switch (in) {
    case ItemId::RAW_IRON:   return ItemId::IRON_INGOT;
    case ItemId::RAW_COPPER: return ItemId::COPPER_INGOT;
    case ItemId::RAW_GOLD:   return ItemId::GOLD_INGOT;
    default: break;
  }
  if (in == block(BlockType::SAND)) return block(BlockType::GLASS);
  if (in == block(BlockType::COBBLESTONE)) return block(BlockType::STONE);
  if (in == block(BlockType::SANDSTONE)) return block(BlockType::BRICKS);
  if (isLog(in)) return ItemId::CHARCOAL;
  return std::nullopt;
}

int fuelMs(ItemId fuel) {
  if (fuel == ItemId::COAL || fuel == ItemId::CHARCOAL) return 8 * kSmeltMs;
  if (isLog(fuel)) return 2 * kSmeltMs;
  if (isPlanks(fuel) || fuel == block(BlockType::CRAFTING_TABLE)) return kSmeltMs * 3 / 2;
  if (fuel == ItemId::STICKS) return kSmeltMs / 2;
  return 0;
}

FurnaceStep tickFurnace(Furnace &f, int ms) {
  FurnaceStep step;
  const bool was_lit = f.lit();
  int left = ms;
  while (left > 0) {
    if (!f.lit()) {
      // Light only when it would cook something.
      if (!canCook(f) || f.fuel.empty() || fuelMs(f.fuel.id) == 0) break;
      f.burn_ms = f.burn_total_ms = fuelMs(f.fuel.id);
      split(f.fuel, 1);
      step.slots_changed = true;
    }
    int run = std::min(left, f.burn_ms);
    if (canCook(f)) {
      int need = kSmeltMs - f.cook_ms;
      if (run >= need) {
        run = need;
        ItemStack done{*smeltResult(f.input.id), 1};
        split(f.input, 1);
        mergeInto(f.output, done);
        f.cook_ms = 0;
        step.slots_changed = true;
      } else {
        f.cook_ms += run;
      }
    }
    f.burn_ms -= run;
    left -= run;
  }
  if (!f.lit()) f.burn_total_ms = 0;
  // Nothing left to cook: start the next item from scratch.
  if (f.input.empty()) f.cook_ms = 0;
  step.lit_changed = was_lit != f.lit();
  return step;
}
