#pragma once

#include "block/block_type.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <vector>
#include <string_view>

// Item ids 1..255 are block items: ItemId(n) places BlockType(n), so they are
// not enumerated here and every new block is automatically an item.
// Non-block items start at 256.
enum class ItemId : uint16_t {
  NONE = 0, // == BlockType::AIR; the empty item
  COAL = 256,
  DIAMOND,
  EMERALD,
  RUBY,
  RAW_IRON,
  RAW_GOLD,
  RAW_COPPER,
  WOOL,          // animal products
  BERRIES,
  SEA_SHELL,
  SWEET_PETALS,
  // Append only: the numeric value is persisted in inventory saves.
};

constexpr ItemId kFirstMaterialItem = ItemId::COAL;
constexpr ItemId kLastMaterialItem = ItemId::SWEET_PETALS;

struct ItemIcon {
  enum class Atlas : uint8_t { Block, Item };
  Atlas atlas = Atlas::Block;
  glm::ivec2 tile{0}; // 32 px tile coords within that atlas
};

struct ItemDef {
  std::string_view name;
  uint16_t max_stack = 64;
  ItemIcon icon;
  std::optional<BlockType> places; // set for block items
  int food_heal = 0;                // health points restored when eaten; 0 = not food
};

constexpr uint16_t kDefaultMaxStack = 64;

inline ItemId itemForBlock(BlockType b) {
  return static_cast<ItemId>(static_cast<uint16_t>(b));
}

// True for every item this build knows: placeable blocks and materials.
bool isValidItem(ItemId id);
// Precondition: isValidItem(id). Unknown ids get a placeholder "Unknown" def.
const ItemDef &itemDef(ItemId id);

// What the creative palette offers: every item except blocks that can't be
// broken again (bedrock, water), in id order.
const std::vector<ItemId> &paletteItems();

// What breaking a block yields (always a single item), or nothing.
std::optional<ItemId> dropFor(BlockType b);
