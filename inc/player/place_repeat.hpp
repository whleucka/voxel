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

// Holding the place button keeps building, the way Bedrock edition does it:
// every block of one hold goes on the same kind of face as the first (all on
// tops, or all on sides facing the same way). Walking backward while aiming
// at the side of the last block lays a row or a bridge. Sweeping over the
// ground lays blocks on its top.

// Where a block aimed at with `ray` would go: tall grass and the like are
// built over, anything else gets the block on the face that was hit. A side
// hit in its upper half says so, for a slab to go in the top of its cell.
inline PlaceBlock placementFor(const RaycastResult &ray) {
  if (isReplaceable(ray.block_type)) return PlaceBlock{ray.block_pos, {0, -1, 0}};
  const bool upper = ray.normal.y == 0 && ray.point.y - static_cast<float>(ray.block_pos.y) >= 0.5f;
  return PlaceBlock{ray.block_pos + ray.normal, -ray.normal, upper};
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
  static constexpr float kInterval = 0.2f;     // seconds between held placements, walking
  static constexpr float kMinInterval = 0.05f; // the quickest, flying flat out
  static constexpr float kStride = 0.9f;       // blocks moved per placement, at most

  // A hold that began by placing `first`.
  explicit PlaceRepeat(const PlaceBlock &first)
      : against_(first.against), upper_(first.upper), last_(first.pos) {}

  // Seconds between placements for a builder moving at `speed` blocks/s: a
  // block about every block moved, so flying (three times a walk) doesn't
  // outrun the row it's laying and leave gaps; a walk or less, kInterval.
  static float intervalAt(float speed) {
    return speed > 0.0f ? std::clamp(kStride / speed, kMinInterval, kInterval) : kInterval;
  }

  // Time passes with the aim on `ray` (nullopt: aimed at nothing, or at
  // something not to build on) and the builder's eye at `eye`. Returns the
  // next block to place, once one is due and the aim is on the same kind of
  // face the hold began on. A due block waits for the aim rather than the
  // clock, so sweeping onto a good spot places at once.
  std::optional<PlaceBlock> update(float dt, const std::optional<RaycastResult> &ray,
                                   std::optional<glm::vec3> eye = std::nullopt) {
    float speed = 0.0f;
    if (eye && eye_ && dt > 0.0f) speed = glm::length(*eye - *eye_) / dt;
    eye_ = eye;
    const float interval = intervalAt(speed);
    timer_ = std::min(timer_ + dt, kInterval);
    if (timer_ < interval || !ray || !ray->hit) return std::nullopt;
    PlaceBlock next = placementFor(*ray);
    if (next.against != against_ || next.pos == last_) return std::nullopt;
    timer_ = 0.0f;
    last_ = next.pos;
    next.upper = upper_; // the whole hold's slabs go in the same half
    return next;
  }

private:
  glm::ivec3 against_; // the face the first block went on (as PlaceBlock::against)
  bool upper_;         // and the half of it (PlaceBlock::upper)
  glm::ivec3 last_;
  float timer_ = 0.0f;
  std::optional<glm::vec3> eye_; // where the eye was last update
};
