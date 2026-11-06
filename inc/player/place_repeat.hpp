#pragma once

#include "block/bed.hpp"
#include "block/block_type.hpp"
#include "block/door.hpp"
#include "block/fence.hpp"
#include "block/plant.hpp"
#include "sim/protocol.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <glm/glm.hpp>
#include <optional>

// Holding the place button keeps building. Every block of one hold goes in
// the layer the first one did (the same height for a floor, the same plane
// for a wall), so sweeping the aim paints a strip or a slab instead of
// stacking a pillar toward the player's face.

// Where a block aimed at with `ray` would go: tall grass and the like are
// built over, anything else gets the block on the face that was hit.
inline PlaceBlock placementFor(const RaycastResult &ray) {
  if (isReplaceable(ray.block_type)) return PlaceBlock{ray.block_pos, {0, -1, 0}};
  return PlaceBlock{ray.block_pos + ray.normal, -ray.normal};
}

// Blocks a click uses rather than builds on (crouch to build on them). A hold
// never uses them: sweeping past a door shouldn't swing it.
inline bool usedOnClick(BlockType b) {
  return isBed(b) || isDoor(b) || isGate(b) || isBerryBush(b) || isCake(b) ||
         b == BlockType::CRAFTING_TABLE || b == BlockType::FURNACE ||
         b == BlockType::FURNACE_LIT || b == BlockType::CHEST || b == BlockType::TREASURE_CHEST;
}

class PlaceRepeat {
public:
  static constexpr float kInterval = 0.2f; // seconds between held placements

  // A hold that began by placing `first`.
  explicit PlaceRepeat(const PlaceBlock &first) : last_(first.pos) {
    for (int i = 0; i < 3; ++i)
      if (first.against[i] != 0) axis_ = i;
    layer_ = first.pos[axis_];
  }

  // Time passes with the aim on `ray` (nullopt: aimed at nothing, or at
  // something not to build on). Returns the next block to place, once one is
  // due and the aim gives a spot in the layer. A due block waits for the aim
  // rather than the clock, so sweeping onto a good spot places at once.
  std::optional<PlaceBlock> update(float dt, const std::optional<RaycastResult> &ray) {
    timer_ = std::min(timer_ + dt, kInterval);
    if (timer_ < kInterval || !ray || !ray->hit) return std::nullopt;
    PlaceBlock next = placementFor(*ray);
    if (next.pos[axis_] != layer_ || next.pos == last_) return std::nullopt;
    timer_ = 0.0f;
    last_ = next.pos;
    return next;
  }

private:
  int axis_ = 1;  // the axis the first block was placed along (0 x, 1 y, 2 z)
  int layer_ = 0; // its coordinate on that axis: every block of the hold shares it
  glm::ivec3 last_;
  float timer_ = 0.0f;
};
