#pragma once

#include "block/block_type.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>

// Wild plants and the other small things that sit on a block: cactus,
// torches and cake. Each needs its support (the block under it, or for a wall
// torch the block it's on) and pops off when that goes.

inline bool isPlant(BlockType b) { return b >= BlockType::TALL_GRASS && b <= BlockType::SHELLS; }

inline bool isFlower(BlockType b) { return b >= BlockType::POPPY && b <= BlockType::HIBISCUS; }

inline bool isMushroom(BlockType b) {
  return b == BlockType::RED_MUSHROOM || b == BlockType::BROWN_MUSHROOM;
}

inline bool isBerryBush(BlockType b) {
  return b == BlockType::BERRY_BUSH || b == BlockType::BERRY_BUSH_BARE;
}

inline bool isCoral(BlockType b) { return b >= BlockType::PINK_CORAL && b <= BlockType::YELLOW_CORAL; }

inline bool isTorch(BlockType b) { return b >= BlockType::TORCH && b <= BlockType::TORCH_W; }

// The way from a torch to the block holding it: down for one on the floor.
inline glm::ivec3 torchSupport(BlockType torch) {
  switch (torch) {
    case BlockType::TORCH_N: return {0, 0, -1};
    case BlockType::TORCH_E: return {1, 0, 0};
    case BlockType::TORCH_S: return {0, 0, 1};
    case BlockType::TORCH_W: return {-1, 0, 0};
    default:                 return {0, -1, 0};
  }
}

// The torch held by the block `toward` it (a unit step), if it can hang
// there: on a floor or a wall, never under a ceiling.
inline std::optional<BlockType> torchFor(glm::ivec3 toward) {
  if (toward == glm::ivec3(0, -1, 0)) return BlockType::TORCH;
  if (toward == glm::ivec3(0, 0, -1)) return BlockType::TORCH_N;
  if (toward == glm::ivec3(1, 0, 0)) return BlockType::TORCH_E;
  if (toward == glm::ivec3(0, 0, 1)) return BlockType::TORCH_S;
  if (toward == glm::ivec3(-1, 0, 0)) return BlockType::TORCH_W;
  return std::nullopt;
}

constexpr int kCakeSlices = 4;
inline bool isCake(BlockType b) { return b >= BlockType::CAKE_4 && b <= BlockType::CAKE_1; }
inline int cakeSlices(BlockType cake) {
  return kCakeSlices - (static_cast<int>(cake) - static_cast<int>(BlockType::CAKE_4));
}
// A cake with `slices` left (1..4).
inline BlockType cakeBlock(int slices) {
  return static_cast<BlockType>(static_cast<int>(BlockType::CAKE_4) + kCakeSlices - slices);
}
constexpr float kCakeHeight = 7.0f / 16.0f;

// Whatever pops off when its support goes.
inline bool needsSupport(BlockType b) {
  return isPlant(b) || b == BlockType::CACTUS || isTorch(b) || isCake(b);
}

// Whether `plant` (or cactus, a torch or cake) can stand on `ground`.
// `solid` says whether the ground is a full, solid block.
inline bool growsOn(BlockType plant, BlockType ground, bool solid) {
  using enum BlockType;
  switch (plant) {
    case DEAD_BUSH:
    case SHELLS:
      return ground == SAND;
    case CACTUS:
      return ground == SAND || ground == CACTUS;
    case RED_MUSHROOM:
    case BROWN_MUSHROOM:
      return solid;
    default:
      if (isPlant(plant)) return ground == GRASS || ground == DIRT;
      return solid; // torches and cake
  }
}
