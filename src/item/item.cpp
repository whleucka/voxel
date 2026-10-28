#include "item/item.hpp"
#include "block/block_data.hpp"
#include "block/crop.hpp"
#include <array>

namespace {

constexpr uint16_t kMaterialCount = static_cast<uint16_t>(kLastMaterialItem) -
                                    static_cast<uint16_t>(kFirstMaterialItem) + 1;

// Material items, in ItemId order. Their icon tiles fill the item atlas row
// by row in the same order: tile (i % 8, i / 8) (see
// tools/gen_item_placeholders.py).
struct Material {
  std::string_view name;
  int food_heal;
  uint16_t max_stack = kDefaultMaxStack;
};
constexpr std::array<Material, kMaterialCount> kMaterials = {{
    {"Coal", 0},     {"Diamond", 0},  {"Emerald", 0},   {"Ruby", 0},
    {"Raw Iron", 0}, {"Raw Gold", 0}, {"Raw Copper", 0},
    {"Wool", 0},     {"Berries", 4},  {"Sea Shell", 0}, {"Sweet Petals", 6},
    {"Sticks", 0},
    {"Iron Ingot", 0}, {"Copper Ingot", 0}, {"Gold Ingot", 0}, {"Charcoal", 0},
    // Tools: one to a stack.
    {"Wooden Pickaxe", 0, 1},
    {"Stone Pickaxe", 0, 1},
    {"Copper Pickaxe", 0, 1},
    {"Iron Pickaxe", 0, 1},
    {"Ruby Pickaxe", 0, 1},
    {"Emerald Pickaxe", 0, 1},
    {"Diamond Pickaxe", 0, 1},
    {"Wooden Axe", 0, 1},
    {"Stone Axe", 0, 1},
    {"Copper Axe", 0, 1},
    {"Iron Axe", 0, 1},
    {"Ruby Axe", 0, 1},
    {"Emerald Axe", 0, 1},
    {"Diamond Axe", 0, 1},
    {"Wooden Shovel", 0, 1},
    {"Stone Shovel", 0, 1},
    {"Copper Shovel", 0, 1},
    {"Iron Shovel", 0, 1},
    {"Ruby Shovel", 0, 1},
    {"Emerald Shovel", 0, 1},
    {"Diamond Shovel", 0, 1},
    {"Wooden Sword", 0, 1},
    {"Stone Sword", 0, 1},
    {"Copper Sword", 0, 1},
    {"Iron Sword", 0, 1},
    {"Ruby Sword", 0, 1},
    {"Emerald Sword", 0, 1},
    {"Diamond Sword", 0, 1},
    // Fishing: the rod wears like a tool; the boot is a joke catch.
    {"Fishing Rod", 0, 1},
    {"Raw Pebblefish", 2},
    {"Cooked Pebblefish", 6},
    {"Raw Sunfin", 2},
    {"Cooked Sunfin", 6},
    {"Raw Glowfin", 2},
    {"Cooked Glowfin", 8},
    {"Soggy Boot", 0, 1},
    {"Treasure Key", 0, 16}, // opens a treasure chest
    {"Wooden Door", 0, 16},
    {"White Bed", 0, 16},
    {"Red Bed", 0, 16},
    {"Orange Bed", 0, 16},
    {"Yellow Bed", 0, 16},
    {"Green Bed", 0, 16},
    {"Blue Bed", 0, 16},
    {"Purple Bed", 0, 16},
    {"Pink Bed", 0, 16},
}};
constexpr int kItemAtlasCols = 8;

bool isPlaceableBlock(uint16_t v) {
  if (v == 0 || v > static_cast<uint16_t>(kLastBlockType)) return false;
  // Water is never held, a lit furnace is a furnace that's burning, a
  // treasure chest is only ever found, door and bed halves come from a door
  // or a bed, farmland from a hoe and crops from seeds.
  BlockType b = static_cast<BlockType>(v);
  return b != BlockType::WATER && b != BlockType::FURNACE_LIT &&
         b != BlockType::TREASURE_CHEST && !isDoor(b) && !isBed(b) && !isFarmland(b) &&
         !isCrop(b);
}

bool isMaterial(uint16_t v) {
  return v >= static_cast<uint16_t>(kFirstMaterialItem) &&
         v <= static_cast<uint16_t>(kLastMaterialItem);
}

// Block items are derived from the block's name and side tile; built once so
// itemDef() can hand out stable references.
const std::array<ItemDef, 256> &blockItemDefs() {
  static const std::array<ItemDef, 256> defs = [] {
    std::array<ItemDef, 256> d{};
    for (uint16_t v = 0; v < 256; ++v) {
      if (!isPlaceableBlock(v)) continue;
      BlockType b = static_cast<BlockType>(v);
      auto tex = block_data.find(b);
      glm::ivec2 tile = tex != block_data.end() ? tex->second.side : glm::ivec2{0};
      d[v] = {blockName(b), kDefaultMaxStack, {ItemIcon::Atlas::Block, tile}, b};
    }
    return d;
  }();
  return defs;
}

const std::array<ItemDef, kMaterialCount> &materialDefs() {
  static const std::array<ItemDef, kMaterialCount> defs = [] {
    std::array<ItemDef, kMaterialCount> d{};
    for (uint16_t i = 0; i < kMaterialCount; ++i)
      d[i] = {kMaterials[i].name, kMaterials[i].max_stack,
              {ItemIcon::Atlas::Item, {i % kItemAtlasCols, i / kItemAtlasCols}},
              std::nullopt, kMaterials[i].food_heal};
    return d;
  }();
  return defs;
}

} // namespace

bool isValidItem(ItemId id) {
  uint16_t v = static_cast<uint16_t>(id);
  return isPlaceableBlock(v) || isMaterial(v);
}

const ItemDef &itemDef(ItemId id) {
  static const ItemDef kUnknown{"Unknown", kDefaultMaxStack, {}, std::nullopt};
  uint16_t v = static_cast<uint16_t>(id);
  if (isPlaceableBlock(v)) return blockItemDefs()[v];
  if (isMaterial(v))
    return materialDefs()[v - static_cast<uint16_t>(kFirstMaterialItem)];
  return kUnknown;
}

std::optional<ItemId> dropFor(BlockType b) {
  switch (b) {
    case BlockType::AIR:
    case BlockType::WATER:
    case BlockType::BEDROCK:
    case BlockType::TREASURE_CHEST: return std::nullopt;
    case BlockType::GRASS:
    case BlockType::SNOW_DIRT:
    case BlockType::FARMLAND_DRY:
    case BlockType::FARMLAND_WET: return itemForBlock(BlockType::DIRT);
    case BlockType::STONE:       return itemForBlock(BlockType::COBBLESTONE);
    case BlockType::COAL_ORE:    return ItemId::COAL;
    case BlockType::DIAMOND_ORE: return ItemId::DIAMOND;
    case BlockType::EMERALD_ORE: return ItemId::EMERALD;
    case BlockType::RUBY_ORE:    return ItemId::RUBY;
    case BlockType::IRON_ORE:    return ItemId::RAW_IRON;
    case BlockType::GOLD_ORE:    return ItemId::RAW_GOLD;
    case BlockType::COPPER_ORE:  return ItemId::RAW_COPPER;
    case BlockType::FURNACE_LIT: return itemForBlock(BlockType::FURNACE);
    default:
      // One door from its bottom half; the Game clears the top with it.
      if (isDoor(b)) return doorTop(b) ? std::nullopt : std::optional(ItemId::WOODEN_DOOR);
      // Likewise one bed, of its colour, from its foot.
      if (isBed(b)) return bedHead(b) ? std::nullopt : std::optional(bedItem(bedColour(b)));
      // A crop's harvest is the Game's (item/farming.hpp).
      if (isCrop(b)) return std::nullopt;
      return itemForBlock(b);
  }
}

const std::vector<ItemId> &paletteItems() {
  static const std::vector<ItemId> items = [] {
    std::vector<ItemId> out;
    for (uint16_t v = 1; v <= static_cast<uint16_t>(kLastMaterialItem); ++v) {
      ItemId id = static_cast<ItemId>(v);
      if (!isValidItem(id) || id == itemForBlock(BlockType::BEDROCK) ||
          id == itemForBlock(BlockType::WATER))
        continue;
      out.push_back(id);
    }
    return out;
  }();
  return items;
}
