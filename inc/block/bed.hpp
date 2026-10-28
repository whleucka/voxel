#pragma once

#include "block/block_type.hpp"
#include "block/door.hpp"
#include <glm/glm.hpp>

// Beds: two cells long (a foot and a head), with their whole state in the
// block type like doors. The 64 types run from BED_WHITE_FOOT_N in the order
// colour * 8 + head * 4 + facing. Facing is the way the placer looked, which
// is the way from the foot to the head.

// The coloured blocks' order (WHITE_BLOCK .. PINK_BLOCK).
enum class BedColour : uint8_t { White, Red, Orange, Yellow, Green, Blue, Purple, Pink };
constexpr int kBedColours = 8;

// How tall a bed stands: what feet land on, and what the mesh draws.
constexpr float kBedHeight = 9.0f / 16.0f;

inline uint8_t bedIndex(BlockType b) {
  return static_cast<uint8_t>(b) - static_cast<uint8_t>(BlockType::BED_WHITE_FOOT_N);
}

inline bool isBed(BlockType b) {
  return b >= BlockType::BED_WHITE_FOOT_N && b <= BlockType::BED_PINK_HEAD_W;
}
inline BedColour bedColour(BlockType b) { return static_cast<BedColour>(bedIndex(b) / 8); }
inline bool bedHead(BlockType b) { return bedIndex(b) % 8 >= 4; }
inline DoorFacing bedFacing(BlockType b) { return static_cast<DoorFacing>(bedIndex(b) % 4); }

inline BlockType bedBlock(BedColour colour, bool head, DoorFacing facing) {
  return static_cast<BlockType>(static_cast<uint8_t>(BlockType::BED_WHITE_FOOT_N) +
                                static_cast<uint8_t>(colour) * 8 + (head ? 4 : 0) +
                                static_cast<uint8_t>(facing));
}

// One cell along a facing: N -Z, E +X, S +Z, W -X.
inline glm::ivec3 facingStep(DoorFacing facing) {
  switch (facing) {
    case DoorFacing::North: return {0, 0, -1};
    case DoorFacing::East:  return {1, 0, 0};
    case DoorFacing::South: return {0, 0, 1};
    case DoorFacing::West:
    default:                return {-1, 0, 0};
  }
}

// The cell holding the other half of the bed at `pos`.
inline glm::ivec3 bedOther(glm::ivec3 pos, BlockType b) {
  glm::ivec3 step = facingStep(bedFacing(b));
  return bedHead(b) ? pos - step : pos + step;
}

// The cell holding the foot of the bed at `pos`.
inline glm::ivec3 bedFoot(glm::ivec3 pos, BlockType b) {
  return bedHead(b) ? bedOther(pos, b) : pos;
}
