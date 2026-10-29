#include "item/recipes.hpp"
#include "block/block_type.hpp"
#include "block/stairs.hpp"
#include "item/tools.hpp"
#include <algorithm>
#include <limits>

namespace {

ItemId block(BlockType b) { return itemForBlock(b); }

std::vector<ItemId> anyPlanks() {
  return {block(BlockType::OAK_PLANKS), block(BlockType::PINE_PLANKS),
          block(BlockType::PALM_PLANKS), block(BlockType::CHERRY_PLANKS)};
}

Recipe anywhere(ItemStack makes, std::vector<Ingredient> needs) {
  return {makes, std::move(needs), false};
}

Recipe atTable(ItemStack makes, std::vector<Ingredient> needs) {
  return {makes, std::move(needs), true};
}

Recipe planks(BlockType log, BlockType planks) {
  return anywhere({block(planks), 4}, {{{block(log)}, 1}});
}

Recipe dyed(BlockType colour, ItemId dye) {
  return atTable({block(colour), 8},
                 {{{block(BlockType::WHITE_BLOCK)}, 8}, {{dye}, 1}});
}

} // namespace

const std::vector<Recipe> &recipes() {
  // Append only: the index is the recipe's id on the wire.
  static const std::vector<Recipe> all = [] {
    std::vector<Recipe> list = {
      planks(BlockType::OAK_LOG, BlockType::OAK_PLANKS),
      planks(BlockType::PINE_LOG, BlockType::PINE_PLANKS),
      planks(BlockType::PALM_LOG, BlockType::PALM_PLANKS),
      planks(BlockType::CHERRY_LOG, BlockType::CHERRY_PLANKS),
      anywhere({ItemId::STICKS, 4}, {{anyPlanks(), 2}}),
      anywhere({block(BlockType::CRAFTING_TABLE), 1}, {{anyPlanks(), 4}}),
      atTable({block(BlockType::STONE_BRICKS), 4}, {{{block(BlockType::COBBLESTONE)}, 4}}),
      // White from sand and anything white; then each colour from its
      // flower. (Gems, ores and creature drops were the dyes before plants.)
      atTable({block(BlockType::WHITE_BLOCK), 8},
              {{{block(BlockType::SAND)}, 8},
               {{ItemId::WOOL, block(BlockType::DAISY), ItemId::SEA_SHELL}, 1}}),
      dyed(BlockType::RED_BLOCK, block(BlockType::POPPY)),
      dyed(BlockType::ORANGE_BLOCK, block(BlockType::HIBISCUS)),
      dyed(BlockType::YELLOW_BLOCK, block(BlockType::BUTTERCUP)),
      dyed(BlockType::GREEN_BLOCK, block(BlockType::CACTUS)),
      dyed(BlockType::BLUE_BLOCK, block(BlockType::CORNFLOWER)),
      dyed(BlockType::PURPLE_BLOCK, block(BlockType::LAVENDER)),
      dyed(BlockType::PINK_BLOCK, block(BlockType::PINK_TULIP)),
      atTable({block(BlockType::GLOWSTONE), 1},
              {{{block(BlockType::GLOW_CRYSTAL)}, 1}, {{ItemId::COPPER_INGOT}, 1}}),
      atTable({block(BlockType::FURNACE), 1}, {{{block(BlockType::COBBLESTONE)}, 8}}),
    };
    // Tools, kind by kind and tier by tier, at a table: a head of the
    // material and a handle of sticks.
    struct Shape { ToolKind kind; uint16_t head, sticks; };
    const Shape shapes[] = {{ToolKind::Pickaxe, 3, 2}, {ToolKind::Axe, 3, 2},
                            {ToolKind::Shovel, 1, 2}, {ToolKind::Sword, 2, 1}};
    const std::pair<Tier, std::vector<ItemId>> materials[] = {
        {Tier::Wood, anyPlanks()},
        {Tier::Stone, {block(BlockType::COBBLESTONE)}},
        {Tier::Copper, {ItemId::COPPER_INGOT}},
        {Tier::Iron, {ItemId::IRON_INGOT}},
        {Tier::Ruby, {ItemId::RUBY}},
        {Tier::Emerald, {ItemId::EMERALD}},
        {Tier::Diamond, {ItemId::DIAMOND}}};
    for (const Shape &shape : shapes)
      for (const auto &[tier, material] : materials)
        list.push_back(atTable({toolItem(shape.kind, tier), 1},
                               {{material, shape.head}, {{ItemId::STICKS}, shape.sticks}}));
    list.push_back(atTable({ItemId::FISHING_ROD, 1},
                           {{{ItemId::STICKS}, 3}, {{ItemId::WOOL}, 2}}));
    list.push_back(atTable({block(BlockType::CHEST), 1}, {{anyPlanks(), 8}}));
    list.push_back(atTable({ItemId::TREASURE_KEY, 1},
                           {{{ItemId::GOLD_INGOT}, 2},
                            {{ItemId::RUBY, ItemId::EMERALD, ItemId::DIAMOND}, 1}}));
    list.push_back(atTable({ItemId::WOODEN_DOOR, 2}, {{anyPlanks(), 6}}));
    // A white bed, then dyed with the coloured blocks' dyes.
    list.push_back(atTable({ItemId::WHITE_BED, 1}, {{{ItemId::WOOL}, 3}, {anyPlanks(), 3}}));
    const std::pair<BedColour, ItemId> dyes[] = {
        {BedColour::Red, block(BlockType::POPPY)},
        {BedColour::Orange, block(BlockType::HIBISCUS)},
        {BedColour::Yellow, block(BlockType::BUTTERCUP)},
        {BedColour::Green, block(BlockType::CACTUS)},
        {BedColour::Blue, block(BlockType::CORNFLOWER)},
        {BedColour::Purple, block(BlockType::LAVENDER)},
        {BedColour::Pink, block(BlockType::PINK_TULIP)}};
    for (const auto &[colour, dye] : dyes)
      list.push_back(atTable({bedItem(colour), 1}, {{{ItemId::WHITE_BED}, 1}, {{dye}, 1}}));
    // Farming: hoes like the other tools, bread from wheat, strawberry seeds
    // from the berries animals give, and a can of copper.
    for (const auto &[tier, material] : materials)
      list.push_back(atTable({toolItem(ToolKind::Hoe, tier), 1},
                             {{material, 2}, {{ItemId::STICKS}, 2}}));
    list.push_back(atTable({ItemId::BREAD, 1}, {{{ItemId::WHEAT}, 3}}));
    list.push_back(atTable({ItemId::STRAWBERRY_SEEDS, 2}, {{{ItemId::BERRIES}, 1}}));
    list.push_back(atTable({ItemId::WATERING_CAN, 1}, {{{ItemId::COPPER_INGOT}, 3}}));
    // A boat, made anywhere so it can be made at the shore.
    list.push_back(anywhere({ItemId::BOAT, 1}, {{anyPlanks(), 5}}));
    // Biome plants: torches made anywhere (you'll want them in a cave),
    // bricks for the sand and snow, compost from leaves, and food.
    list.push_back(anywhere({block(BlockType::TORCH), 4},
                            {{{ItemId::COAL, ItemId::CHARCOAL}, 1}, {{ItemId::STICKS}, 1}}));
    list.push_back(atTable({block(BlockType::SANDSTONE_BRICKS), 4},
                           {{{block(BlockType::SANDSTONE)}, 4}}));
    list.push_back(atTable({block(BlockType::SNOW_BRICKS), 4}, {{{block(BlockType::SNOW)}, 4}}));
    list.push_back(anywhere({ItemId::COMPOST, 1},
                            {{{block(BlockType::OAK_LEAF), block(BlockType::PINE_LEAF),
                               block(BlockType::PALM_LEAF), block(BlockType::CHERRY_LEAF)},
                              4}}));
    list.push_back(atTable({ItemId::MUSHROOM_STEW, 1},
                           {{{block(BlockType::RED_MUSHROOM)}, 1},
                            {{block(BlockType::BROWN_MUSHROOM)}, 1}}));
    list.push_back(atTable({ItemId::BERRY_COOKIE, 4},
                           {{{ItemId::WHEAT}, 2}, {{ItemId::BERRIES}, 1}}));
    list.push_back(atTable({block(BlockType::CAKE_4), 1},
                           {{{ItemId::WHEAT}, 3}, {{ItemId::STRAWBERRY}, 2},
                            {{ItemId::SWEET_PETALS}, 1}}));
    // Stairs and slabs of each building material: 6 make 4 stairs, and 3
    // make 6 slabs (which make the 3 blocks again, stacked in pairs).
    for (int m = 0; m < kShapeMaterials; ++m) {
      const ItemId material = block(kShapeMaterial[m]);
      list.push_back(atTable({block(stairBlock(m, DoorFacing::North)), 4}, {{{material}, 6}}));
      list.push_back(atTable({block(slabBlock(m)), 6}, {{{material}, 3}}));
    }
    return list;
  }();
  return all;
}

bool knows(const Recipe &recipe, const SeenItems &seen) {
  return std::all_of(recipe.needs.begin(), recipe.needs.end(), [&](const Ingredient &in) {
    return std::any_of(in.any_of.begin(), in.any_of.end(), [&](ItemId id) {
      auto v = static_cast<size_t>(id);
      return v < kItemIdSpace && seen.test(v);
    });
  });
}

int countOf(const Inventory &inventory, const Ingredient &ingredient) {
  int n = 0;
  for (const ItemStack &s : inventory.slots())
    if (std::find(ingredient.any_of.begin(), ingredient.any_of.end(), s.id) !=
        ingredient.any_of.end())
      n += s.count;
  return n;
}

int timesAffordable(const Recipe &recipe, const Inventory &inventory) {
  int times = std::numeric_limits<int>::max();
  for (const Ingredient &in : recipe.needs)
    times = std::min(times, countOf(inventory, in) / std::max<int>(1, in.count));
  return recipe.needs.empty() ? 0 : times;
}

std::optional<Inventory> craftOnce(const Recipe &recipe, const Inventory &inventory) {
  if (timesAffordable(recipe, inventory) < 1) return std::nullopt;
  Inventory after = inventory;
  for (const Ingredient &in : recipe.needs) {
    int left = in.count;
    for (int i = kInventorySlots - 1; i >= 0 && left > 0; --i) {
      const ItemStack &s = after.slot(i);
      if (std::find(in.any_of.begin(), in.any_of.end(), s.id) == in.any_of.end()) continue;
      left -= after.take(i, static_cast<uint16_t>(std::min<int>(left, s.count))).count;
    }
  }
  if (!after.add(recipe.makes).empty()) return std::nullopt;
  return after;
}
