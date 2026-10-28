#pragma once

#include "block/block_type.hpp"
#include <cstdint>
#include <glm/glm.hpp>

// Farming blocks: tilled soil, dry or wet, and crops growing on it. A crop's
// kind and stage are its block type, so saves and BlockChanged carry them
// like any other block. The 12 crop types run from WHEAT_0 in the order
// kind * 4 + stage.

enum class CropKind : uint8_t { Wheat, Carrot, Strawberry };

constexpr int kCropKinds = 3;
constexpr int kCropStages = 4; // sprout, small, large, ripe

inline bool isFarmland(BlockType b) {
  return b == BlockType::FARMLAND_DRY || b == BlockType::FARMLAND_WET;
}

inline bool isCrop(BlockType b) {
  return b >= BlockType::WHEAT_0 && b <= BlockType::STRAWBERRY_3;
}

inline uint8_t cropIndex(BlockType b) {
  return static_cast<uint8_t>(b) - static_cast<uint8_t>(BlockType::WHEAT_0);
}
inline CropKind cropKind(BlockType b) { return static_cast<CropKind>(cropIndex(b) / kCropStages); }
inline int cropStage(BlockType b) { return cropIndex(b) % kCropStages; }
inline bool cropRipe(BlockType b) { return cropStage(b) == kCropStages - 1; }

inline BlockType cropBlock(CropKind kind, int stage) {
  return static_cast<BlockType>(static_cast<uint8_t>(BlockType::WHEAT_0) +
                                static_cast<uint8_t>(kind) * kCropStages + stage);
}

// Atlas tiles (tools/gen_farming_tiles.py), counted on from (0, 4) after the
// bed blankets and the glow crystal's end.
constexpr glm::ivec2 kFarmlandDryTop{8, 4};
constexpr glm::ivec2 kFarmlandWetTop{9, 4};
constexpr glm::ivec2 kFarmlandDrySide{10, 4};
constexpr glm::ivec2 kFarmlandWetSide{11, 4};
constexpr glm::ivec2 kCropTopTile{12, 4}; // all clear: a crop is seen from the side
inline glm::ivec2 cropTile(BlockType crop) {
  int i = 13 + cropIndex(crop);
  return {i % 16, 4 + i / 16};
}
