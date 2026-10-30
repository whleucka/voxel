#include "item/mining.hpp"
#include "block/bed.hpp"
#include "block/crop.hpp"
#include "block/door.hpp"
#include "block/plant.hpp"
#include "block/stairs.hpp"

Toughness toughness(BlockType b) {
  using enum BlockType;
  switch (b) {
    case AIR:
    case WATER:
    case BEDROCK:
    case TREASURE_CHEST: // only a key opens it
      return {false};

    case OAK_LEAF: case PINE_LEAF: case PALM_LEAF: case CHERRY_LEAF:
      return {true, 300, ToolKind::Axe, ToolKind::Sword};
    case GLASS:
      return {true, 600, ToolKind::Pickaxe, ToolKind::None, Tier::Wood};
    case GLOW_CRYSTAL:
      return {true, 600, ToolKind::Pickaxe};

    case DIRT: case GRASS: case SAND: case SNOW: case SNOW_DIRT:
    case FARMLAND_DRY: case FARMLAND_WET:
      return {true, 750, ToolKind::Shovel};

    case WHITE_BLOCK: case RED_BLOCK: case ORANGE_BLOCK: case YELLOW_BLOCK:
    case GREEN_BLOCK: case BLUE_BLOCK: case PURPLE_BLOCK: case PINK_BLOCK:
    case GLOWSTONE:
      return {true, 1000, ToolKind::Pickaxe};

    case OAK_LOG: case PINE_LOG: case PALM_LOG: case CHERRY_LOG:
    case OAK_PLANKS: case PINE_PLANKS: case PALM_PLANKS: case CHERRY_PLANKS:
    case CRAFTING_TABLE: case CHEST:
      return {true, 3000, ToolKind::Axe};

    case CACTUS:
      return {true, 400, ToolKind::Axe};
    case PINK_CORAL: case BLUE_CORAL: case YELLOW_CORAL: case SNOW_BRICKS:
      return {true, 1000, ToolKind::Pickaxe};

    case STONE: case COBBLESTONE: case SANDSTONE: case SNOW_STONE:
    case BRICKS: case STONE_BRICKS: case SANDSTONE_BRICKS: case FURNACE: case FURNACE_LIT:
      return {true, 7500, ToolKind::Pickaxe, ToolKind::None, Tier::Wood};

    case COAL_ORE: case COPPER_ORE:
      return {true, 10000, ToolKind::Pickaxe, ToolKind::None, Tier::Stone};
    case IRON_ORE: case GOLD_ORE:
      return {true, 10000, ToolKind::Pickaxe, ToolKind::None, Tier::Copper};
    case RUBY_ORE: case EMERALD_ORE: case DIAMOND_ORE:
      return {true, 12000, ToolKind::Pickaxe, ToolKind::None, Tier::Iron};

    default:
      if (isDoor(b) || isBed(b) || isFence(b) || isGate(b))
        return {true, 3000, ToolKind::Axe}; // like planks
      if (isLadder(b)) return {true, 600, ToolKind::Axe};
      if (isHalfShape(b)) return toughness(shapeMaterial(b));
      // Picked at once.
      if (isCrop(b) || isPlant(b) || isTorch(b) || isCake(b)) return {true, 0};
      return {true, 1000};
  }
}

namespace {

bool rightTool(const Toughness &t, ToolKind kind) {
  return kind != ToolKind::None && (kind == t.best || kind == t.also);
}

} // namespace

int breakMs(BlockType block, ItemId held) {
  Toughness t = toughness(block);
  if (!t.breakable) return -1;
  Tool tool = toolOf(held);
  int speed = rightTool(t, tool.kind) ? miningSpeed(tool.tier) : 1;
  return t.hand_ms / speed;
}

bool dropsWith(BlockType block, ItemId held) {
  Toughness t = toughness(block);
  if (!t.breakable) return false;
  if (t.needs == Tier::Hand) return true;
  Tool tool = toolOf(held);
  return tool.kind == t.best && tool.tier >= t.needs;
}

bool isOre(BlockType b) {
  using enum BlockType;
  switch (b) {
    case COAL_ORE: case COPPER_ORE: case IRON_ORE: case GOLD_ORE:
    case RUBY_ORE: case EMERALD_ORE: case DIAMOND_ORE:
      return true;
    default:
      return false;
  }
}

ItemId pickaxeNeeded(BlockType block, ItemId held) {
  if (!isOre(block) || dropsWith(block, held)) return ItemId::NONE;
  return toolItem(ToolKind::Pickaxe, toughness(block).needs);
}
