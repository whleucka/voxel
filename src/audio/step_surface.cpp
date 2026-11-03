#include "audio/step_surface.hpp"
#include "block/bed.hpp"
#include "block/crop.hpp"
#include "block/door.hpp"
#include "block/fence.hpp"
#include "block/plant.hpp"
#include "block/stairs.hpp"

std::optional<StepSet> listedStepSurface(BlockType b) {
  // The ranges first: their members aren't listed in the switch below.
  if (isHalfShape(b)) return listedStepSurface(shapeMaterial(b));
  if (isDoor(b) || isFence(b) || isGate(b) || isLadder(b)) return StepSet::Wood;
  if (isBed(b) || isCake(b)) return StepSet::Cloth;
  if (isFarmland(b) || isCrop(b)) return StepSet::Dirt;
  if (isTorch(b)) return StepSet::Silent;
  if (b == BlockType::SHELLS || b == BlockType::DEAD_BUSH) return StepSet::Dirt;
  if (isPlant(b)) return StepSet::Grass;

  switch (b) {
  case BlockType::AIR:
  case BlockType::WATER:
    return StepSet::Silent;
  case BlockType::GRASS:
  case BlockType::OAK_LEAF:
  case BlockType::PINE_LEAF:
  case BlockType::PALM_LEAF:
  case BlockType::CHERRY_LEAF:
  case BlockType::CACTUS:
    return StepSet::Grass;
  case BlockType::DIRT:
  case BlockType::SAND:
    return StepSet::Dirt;
  case BlockType::BEDROCK:
  case BlockType::COBBLESTONE:
  case BlockType::STONE:
  case BlockType::SANDSTONE:
  case BlockType::COAL_ORE:
  case BlockType::GOLD_ORE:
  case BlockType::DIAMOND_ORE:
  case BlockType::IRON_ORE:
  case BlockType::EMERALD_ORE:
  case BlockType::RUBY_ORE:
  case BlockType::COPPER_ORE:
  case BlockType::GLOWSTONE:
  case BlockType::BRICKS:
  case BlockType::STONE_BRICKS:
  case BlockType::SANDSTONE_BRICKS:
  case BlockType::GLASS:
  case BlockType::GLOW_CRYSTAL:
  case BlockType::FURNACE:
  case BlockType::FURNACE_LIT:
  case BlockType::PINK_CORAL:
  case BlockType::BLUE_CORAL:
  case BlockType::YELLOW_CORAL:
    return StepSet::Stone;
  case BlockType::OAK_LOG:
  case BlockType::PINE_LOG:
  case BlockType::PALM_LOG:
  case BlockType::CHERRY_LOG:
  case BlockType::OAK_PLANKS:
  case BlockType::PINE_PLANKS:
  case BlockType::PALM_PLANKS:
  case BlockType::CHERRY_PLANKS:
  case BlockType::CRAFTING_TABLE:
  case BlockType::CHEST:
  case BlockType::TREASURE_CHEST:
    return StepSet::Wood;
  case BlockType::SNOW:
  case BlockType::SNOW_DIRT:
  case BlockType::SNOW_STONE:
  case BlockType::SNOW_BRICKS:
    return StepSet::Snow;
  case BlockType::WHITE_BLOCK:
  case BlockType::RED_BLOCK:
  case BlockType::ORANGE_BLOCK:
  case BlockType::YELLOW_BLOCK:
  case BlockType::GREEN_BLOCK:
  case BlockType::BLUE_BLOCK:
  case BlockType::PURPLE_BLOCK:
  case BlockType::PINK_BLOCK:
    return StepSet::Cloth;
  default:
    // The ranges above never get here; a new block does, and the test that
    // sweeps every type fails until it's listed.
    return std::nullopt;
  }
}
