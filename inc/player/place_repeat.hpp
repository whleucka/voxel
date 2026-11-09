#pragma once

#include "block/bed.hpp"
#include "block/block_type.hpp"
#include "block/door.hpp"
#include "block/fence.hpp"
#include "block/plant.hpp"
#include "sim/protocol.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

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

// Where a ray from `origin` along `dir` (unit) enters the cell at `cell`
// within `reach`, as a hit on it; nothing if it misses or starts inside.
inline std::optional<RaycastResult> rayIntoCell(glm::vec3 origin, glm::vec3 dir, float reach,
                                                glm::ivec3 cell) {
  float t_in = 0.0f, t_out = reach;
  int axis = -1;
  for (int a = 0; a < 3; ++a) {
    const float lo = static_cast<float>(cell[a]), hi = lo + 1.0f;
    if (std::abs(dir[a]) < 1e-8f) {
      if (origin[a] < lo || origin[a] > hi) return std::nullopt;
      continue;
    }
    float t0 = (lo - origin[a]) / dir[a], t1 = (hi - origin[a]) / dir[a];
    if (t0 > t1) std::swap(t0, t1);
    if (t0 > t_in) t_in = t0, axis = a;
    t_out = std::min(t_out, t1);
    if (t_in > t_out) return std::nullopt;
  }
  if (axis < 0) return std::nullopt; // the ray starts inside it
  RaycastResult r;
  r.hit = true;
  r.block_pos = cell;
  r.normal[axis] = dir[axis] > 0.0f ? -1 : 1;
  r.distance = t_in;
  r.block_type = BlockType::STONE; // stands in for whatever is going there
  r.point = origin + dir * t_in;
  return r;
}

class PlaceRepeat {
public:
  static constexpr float kInterval = 0.2f;      // seconds between held placements, walking
  static constexpr float kMinInterval = 0.025f; // the quickest, flying flat out
  static constexpr float kStride = 0.9f;        // blocks moved per placement, at most
  static constexpr float kPendingSeconds = 0.5f; // how long a sent block is assumed there
  static constexpr float kSweepStep = 0.1f;      // blocks the eye moves per aim sampled

  // Aims a ray from an eye, as World::raycast does, but only at things to
  // build on (nullopt otherwise).
  using Cast = std::function<std::optional<RaycastResult>(glm::vec3 origin, glm::vec3 dir)>;

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

  // A frame of the hold with the eye now at `eye` looking along `front`
  // (unit), aiming with `cast` within `reach`. Blocks this hold sent are aimed
  // at as if already there, since the world only shows them once the Game has
  // put them in, a tick or so later: flying backward along a bridge, the aim
  // is on the newest block's side for only a fraction of a block, and without
  // them it would fall off the end before the block appeared. And the aim is
  // swept from last frame's to this one in short steps, so a quick builder
  // can't skip past that fraction between two frames. Returns what to place.
  std::vector<PlaceBlock> sweep(float dt, glm::vec3 eye, glm::vec3 front, float reach,
                                const Cast &cast) {
    for (auto &p : pending_) p.age += dt;
    while (!pending_.empty() && pending_.front().age > kPendingSeconds) pending_.pop_front();

    const glm::vec3 from_eye = eye_.value_or(eye), from_front = front_.value_or(front);
    front_ = front;
    const float moved = glm::length(eye - from_eye);
    const int steps = std::clamp(static_cast<int>(std::ceil(moved / kSweepStep)), 1, 16);
    std::vector<PlaceBlock> placed;
    for (int i = 1; i <= steps; ++i) {
      const float f = static_cast<float>(i) / static_cast<float>(steps);
      const glm::vec3 o = glm::mix(from_eye, eye, f);
      glm::vec3 d = glm::mix(from_front, front, f);
      d = glm::length(d) > 1e-6f ? glm::normalize(d) : front;
      std::optional<RaycastResult> ray = cast(o, d);
      for (const auto &p : pending_)
        if (auto hit = rayIntoCell(o, d, reach, p.pos);
            hit && (!ray || hit->distance < ray->distance))
          ray = hit;
      if (auto next = update(dt / static_cast<float>(steps), ray, o)) {
        pending_.push_back({next->pos, 0.0f});
        placed.push_back(*next);
      }
    }
    return placed;
  }

  // The first block of the hold was sent too: aim at it before it shows.
  void sent(const PlaceBlock &first) { pending_.push_back({first.pos, 0.0f}); }

private:
  struct Pending {
    glm::ivec3 pos;
    float age; // seconds since it was sent
  };
  glm::ivec3 against_; // the face the first block went on (as PlaceBlock::against)
  bool upper_;         // and the half of it (PlaceBlock::upper)
  glm::ivec3 last_;
  float timer_ = 0.0f;
  std::optional<glm::vec3> eye_;   // where the eye was last update
  std::optional<glm::vec3> front_; // and where it looked, last sweep
  std::deque<Pending> pending_;    // blocks sent, maybe not in the world yet
};
