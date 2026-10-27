#include "item/item.hpp"
#include "block/block_data.hpp"
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
};
constexpr std::array<Material, kMaterialCount> kMaterials = {{
    {"Coal", 0},     {"Diamond", 0},  {"Emerald", 0},   {"Ruby", 0},
    {"Raw Iron", 0}, {"Raw Gold", 0}, {"Raw Copper", 0},
    {"Wool", 0},     {"Berries", 4},  {"Sea Shell", 0}, {"Sweet Petals", 6},
}};
constexpr int kItemAtlasCols = 8;

bool isPlaceableBlock(uint16_t v) {
  if (v == 0 || v > static_cast<uint16_t>(kLastBlockType)) return false;
  return static_cast<BlockType>(v) != BlockType::WATER;
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
      d[i] = {kMaterials[i].name, kDefaultMaxStack,
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
    case BlockType::BEDROCK:     return std::nullopt;
    case BlockType::GRASS:
    case BlockType::SNOW_DIRT:   return itemForBlock(BlockType::DIRT);
    case BlockType::STONE:       return itemForBlock(BlockType::COBBLESTONE);
    case BlockType::COAL_ORE:    return ItemId::COAL;
    case BlockType::DIAMOND_ORE: return ItemId::DIAMOND;
    case BlockType::EMERALD_ORE: return ItemId::EMERALD;
    case BlockType::RUBY_ORE:    return ItemId::RUBY;
    case BlockType::IRON_ORE:    return ItemId::RAW_IRON;
    case BlockType::GOLD_ORE:    return ItemId::RAW_GOLD;
    case BlockType::COPPER_ORE:  return ItemId::RAW_COPPER;
    default:                     return itemForBlock(b);
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
