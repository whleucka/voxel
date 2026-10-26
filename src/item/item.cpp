#include "item/item.hpp"
#include "block/block_data.hpp"
#include <array>

namespace {

constexpr uint16_t kMaterialCount = static_cast<uint16_t>(kLastMaterialItem) -
                                    static_cast<uint16_t>(kFirstMaterialItem) + 1;

// Material items, in ItemId order. Their icon tiles sit in row 0 of the item
// atlas in the same order (see tools/gen_item_placeholders.py).
constexpr std::array<std::string_view, kMaterialCount> kMaterialNames = {
    "Coal", "Diamond", "Emerald", "Ruby", "Raw Iron", "Raw Gold", "Raw Copper",
};

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
      d[i] = {kMaterialNames[i], kDefaultMaxStack,
              {ItemIcon::Atlas::Item, {static_cast<int>(i), 0}}, std::nullopt};
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
