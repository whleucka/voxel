#pragma once

#include "block/block_type.hpp"
#include "block/door.hpp"
#include <glm/glm.hpp>
#include <string_view>

// Slabs and stairs: half-block shapes of nine building materials. A slab
// fills the bottom half of its cell; a stair is a slab with a half-block on
// top at its back, the side its facing points to. Stairs keep their facing in
// the type, as doors do: material * 4 + facing, from OAK_STAIRS_N.

constexpr int kShapeMaterials = 9;

// The full blocks they're made of, in the order of their types.
constexpr BlockType kShapeMaterial[kShapeMaterials] = {
    BlockType::OAK_PLANKS,  BlockType::PINE_PLANKS,  BlockType::PALM_PLANKS,
    BlockType::CHERRY_PLANKS, BlockType::COBBLESTONE, BlockType::STONE_BRICKS,
    BlockType::BRICKS,      BlockType::SANDSTONE_BRICKS, BlockType::SNOW_BRICKS};

inline bool isSlab(BlockType b) { return b >= BlockType::OAK_SLAB && b <= BlockType::SNOW_BRICK_SLAB; }
inline bool isStair(BlockType b) {
  return b >= BlockType::OAK_STAIRS_N && b <= BlockType::SNOW_BRICK_STAIRS_W;
}
inline bool isHalfShape(BlockType b) { return isSlab(b) || isStair(b); }

inline int shapeMaterialIndex(BlockType b) {
  return isSlab(b) ? static_cast<int>(b) - static_cast<int>(BlockType::OAK_SLAB)
                   : (static_cast<int>(b) - static_cast<int>(BlockType::OAK_STAIRS_N)) / 4;
}
// The full block a slab or stair is made of.
inline BlockType shapeMaterial(BlockType b) { return kShapeMaterial[shapeMaterialIndex(b)]; }

inline DoorFacing stairFacing(BlockType b) {
  return static_cast<DoorFacing>((static_cast<int>(b) - static_cast<int>(BlockType::OAK_STAIRS_N)) % 4);
}

inline BlockType slabBlock(int material) {
  return static_cast<BlockType>(static_cast<int>(BlockType::OAK_SLAB) + material);
}
inline BlockType stairBlock(int material, DoorFacing facing) {
  return static_cast<BlockType>(static_cast<int>(BlockType::OAK_STAIRS_N) + material * 4 +
                                static_cast<int>(facing));
}
// The same stair, turned to `facing`.
inline BlockType turned(BlockType stair, DoorFacing facing) {
  return stairBlock(shapeMaterialIndex(stair), facing);
}

constexpr float kSlabHeight = 0.5f;

constexpr std::string_view kSlabNames[kShapeMaterials] = {
    "Oak Slab",          "Pine Slab",  "Palm Slab",
    "Cherry Slab",       "Cobblestone Slab", "Stone Brick Slab",
    "Brick Slab",        "Sandstone Brick Slab", "Snow Brick Slab"};
constexpr std::string_view kStairNames[kShapeMaterials] = {
    "Oak Stairs",        "Pine Stairs", "Palm Stairs",
    "Cherry Stairs",     "Cobblestone Stairs", "Stone Brick Stairs",
    "Brick Stairs",      "Sandstone Brick Stairs", "Snow Brick Stairs"};

// A stair's upper step: the back half of the top of its cell, the way it
// faces (N is -Z, E +X; see facingStep).
inline DoorBox stairStep(DoorFacing facing) {
  constexpr float h = kSlabHeight;
  switch (facing) {
    case DoorFacing::North: return {{0, h, 0}, {1, 1, 0.5f}};
    case DoorFacing::East:  return {{0.5f, h, 0}, {1, 1, 1}};
    case DoorFacing::South: return {{0, h, 0.5f}, {1, 1, 1}};
    case DoorFacing::West:
    default:                return {{0, h, 0}, {0.5f, 1, 1}};
  }
}
