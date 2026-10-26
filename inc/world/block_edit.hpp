#pragma once

#include "block/block_type.hpp"
#include <glm/glm.hpp>

// One player-made block change, in world coordinates.
struct BlockEdit {
  glm::ivec3 pos{0};
  BlockType type = BlockType::AIR;
  bool operator==(const BlockEdit &) const = default;
};
