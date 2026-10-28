#pragma once

#include "block/bed.hpp"
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
  STICKS,        // crafting
  IRON_INGOT,    // smelting
  COPPER_INGOT,
  GOLD_INGOT,
  CHARCOAL,
  // Tools (equipment), kind by kind, each in tier order: see item/tools.hpp.
  WOOD_PICKAXE,
  STONE_PICKAXE,
  COPPER_PICKAXE,
  IRON_PICKAXE,
  RUBY_PICKAXE,
  EMERALD_PICKAXE,
  DIAMOND_PICKAXE,
  WOOD_AXE,
  STONE_AXE,
  COPPER_AXE,
  IRON_AXE,
  RUBY_AXE,
  EMERALD_AXE,
  DIAMOND_AXE,
  WOOD_SHOVEL,
  STONE_SHOVEL,
  COPPER_SHOVEL,
  IRON_SHOVEL,
  RUBY_SHOVEL,
  EMERALD_SHOVEL,
  DIAMOND_SHOVEL,
  WOOD_SWORD,
  STONE_SWORD,
  COPPER_SWORD,
  IRON_SWORD,
  RUBY_SWORD,
  EMERALD_SWORD,
  DIAMOND_SWORD,
  FISHING_ROD,       // fishing
  RAW_PEBBLEFISH,
  COOKED_PEBBLEFISH,
  RAW_SUNFIN,
  COOKED_SUNFIN,
  RAW_GLOWFIN,
  COOKED_GLOWFIN,
  SOGGY_BOOT,
  TREASURE_KEY,
  WOODEN_DOOR,       // places a two-high door (the Game sets its halves)
  WHITE_BED,         // beds: place a two-long bed (the Game sets its halves),
  RED_BED,           // in the coloured blocks' order
  ORANGE_BED,
  YELLOW_BED,
  GREEN_BED,
  BLUE_BED,
  PURPLE_BED,
  PINK_BED,
  WOOD_HOE,          // farming: hoes in tier order (see item/tools.hpp),
  STONE_HOE,
  COPPER_HOE,
  IRON_HOE,
  RUBY_HOE,
  EMERALD_HOE,
  DIAMOND_HOE,
  WHEAT_SEEDS,       // seeds in CropKind order (see item/farming.hpp),
  CARROT_SEEDS,
  STRAWBERRY_SEEDS,
  WHEAT,             // and what grows
  BREAD,
  CARROT,
  STRAWBERRY,
  WATERING_CAN,
  // Append only: the numeric value is persisted in inventory saves.
};

constexpr ItemId kFirstMaterialItem = ItemId::COAL;
constexpr ItemId kLastMaterialItem = ItemId::WATERING_CAN;

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

// The bed item of a colour, and the colour of a bed item.
inline ItemId bedItem(BedColour colour) {
  return static_cast<ItemId>(static_cast<uint16_t>(ItemId::WHITE_BED) +
                             static_cast<uint16_t>(colour));
}
inline bool isBedItem(ItemId id) { return id >= ItemId::WHITE_BED && id <= ItemId::PINK_BED; }
inline BedColour bedItemColour(ItemId id) {
  return static_cast<BedColour>(static_cast<uint16_t>(id) -
                                static_cast<uint16_t>(ItemId::WHITE_BED));
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
