#include "doctest/doctest.h"
#include "audio/step_surface.hpp"
#include "block/bed.hpp"
#include "block/stairs.hpp"

TEST_CASE("every block has a footstep set") {
  for (int v = 0; v <= static_cast<int>(kLastBlockType); ++v) {
    CAPTURE(v);
    CHECK(listedStepSurface(static_cast<BlockType>(v)).has_value());
  }
}

TEST_CASE("footstep sets for each kind of surface") {
  CHECK(stepSurface(BlockType::GRASS) == StepSet::Grass);
  CHECK(stepSurface(BlockType::OAK_LEAF) == StepSet::Grass);
  CHECK(stepSurface(BlockType::TALL_GRASS) == StepSet::Grass);
  CHECK(stepSurface(BlockType::DIRT) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::SAND) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::FARMLAND_WET) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::WHEAT_2) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::SHELLS) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::STONE) == StepSet::Stone);
  CHECK(stepSurface(BlockType::DIAMOND_ORE) == StepSet::Stone);
  CHECK(stepSurface(BlockType::GLASS) == StepSet::Stone);
  CHECK(stepSurface(BlockType::OAK_PLANKS) == StepSet::Wood);
  CHECK(stepSurface(BlockType::PINE_LOG) == StepSet::Wood);
  CHECK(stepSurface(BlockType::LADDER_E) == StepSet::Wood);
  CHECK(stepSurface(BlockType::FENCE_GATE_Z_OPEN) == StepSet::Wood);
  CHECK(stepSurface(BlockType::DOOR_TOP_W_OPEN) == StepSet::Wood);
  CHECK(stepSurface(BlockType::SNOW) == StepSet::Snow);
  CHECK(stepSurface(BlockType::SNOW_DIRT) == StepSet::Snow);
  CHECK(stepSurface(BlockType::RED_BLOCK) == StepSet::Cloth);
  CHECK(stepSurface(BlockType::BED_PINK_HEAD_W) == StepSet::Cloth);
  CHECK(stepSurface(BlockType::CAKE_2) == StepSet::Cloth);
  CHECK(stepSurface(BlockType::AIR) == StepSet::Silent);
  CHECK(stepSurface(BlockType::WATER) == StepSet::Silent);
  CHECK(stepSurface(BlockType::TORCH_N) == StepSet::Silent);
}

TEST_CASE("slabs and stairs sound like what they're made of") {
  CHECK(stepSurface(BlockType::OAK_SLAB) == StepSet::Wood);
  CHECK(stepSurface(BlockType::CHERRY_STAIRS_S) == StepSet::Wood);
  CHECK(stepSurface(BlockType::COBBLESTONE_SLAB) == StepSet::Stone);
  CHECK(stepSurface(BlockType::BRICK_STAIRS_N) == StepSet::Stone);
  CHECK(stepSurface(BlockType::SANDSTONE_BRICK_SLAB) == StepSet::Stone);
  CHECK(stepSurface(BlockType::SNOW_BRICK_STAIRS_W) == StepSet::Snow);
}
