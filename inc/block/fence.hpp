#pragma once

#include "block/block_type.hpp"
#include <glm/glm.hpp>
#include <optional>

// Fences, the fence gate and ladders.
//
// A fence is one type per wood; which sides it reaches to (its rails) comes
// from its neighbours, worked out when it's drawn or bumped into, so it
// needs no type per direction. Fences and closed gates stop bodies
// kFenceHeight high, too tall to jump.
//
// The gate runs along X or along Z (across the way its placer looked), open
// or closed: FENCE_GATE_X_CLOSED + axis * 2 + open.
//
// A ladder hangs on the side of the block to its N, E, S or W, like a wall
// torch; it's walked into and climbed (physics/physics_body.hpp).

constexpr int kFenceWoods = 4;
constexpr float kFenceHeight = 1.5f;

inline bool isFence(BlockType b) { return b >= BlockType::OAK_FENCE && b <= BlockType::CHERRY_FENCE; }
inline int fenceWood(BlockType b) {
  return static_cast<int>(b) - static_cast<int>(BlockType::OAK_FENCE);
}

inline bool isGate(BlockType b) {
  return b >= BlockType::FENCE_GATE_X_CLOSED && b <= BlockType::FENCE_GATE_Z_OPEN;
}
inline int gateIndex(BlockType b) {
  return static_cast<int>(b) - static_cast<int>(BlockType::FENCE_GATE_X_CLOSED);
}
inline bool gateOpen(BlockType b) { return gateIndex(b) % 2 == 1; }
// Runs along X (across a path that goes north-south), else along Z.
inline bool gateAlongX(BlockType b) { return gateIndex(b) < 2; }
inline BlockType gateBlock(bool along_x, bool open) {
  return static_cast<BlockType>(static_cast<int>(BlockType::FENCE_GATE_X_CLOSED) +
                                (along_x ? 0 : 2) + (open ? 1 : 0));
}

inline bool isLadder(BlockType b) { return b >= BlockType::LADDER_N && b <= BlockType::LADDER_W; }

// The way from a ladder to the block it hangs on.
inline glm::ivec3 ladderSupport(BlockType ladder) {
  switch (ladder) {
    case BlockType::LADDER_N: return {0, 0, -1};
    case BlockType::LADDER_E: return {1, 0, 0};
    case BlockType::LADDER_S: return {0, 0, 1};
    case BlockType::LADDER_W:
    default:                  return {-1, 0, 0};
  }
}

// The ladder held by the block `toward` it (a unit step), if that's a side:
// never on a floor or a ceiling.
inline std::optional<BlockType> ladderFor(glm::ivec3 toward) {
  if (toward == glm::ivec3(0, 0, -1)) return BlockType::LADDER_N;
  if (toward == glm::ivec3(1, 0, 0)) return BlockType::LADDER_E;
  if (toward == glm::ivec3(0, 0, 1)) return BlockType::LADDER_S;
  if (toward == glm::ivec3(-1, 0, 0)) return BlockType::LADDER_W;
  return std::nullopt;
}

constexpr const char *kFenceNames[kFenceWoods] = {"Oak Fence", "Pine Fence", "Palm Fence",
                                                  "Cherry Fence"};
