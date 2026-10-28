#pragma once

#include "block/block_type.hpp"
#include <glm/glm.hpp>

// Doors: two blocks tall, with their whole state in the block type, so saves
// and BlockChanged carry it like any other block. The 16 types run from
// DOOR_BOTTOM_N_CLOSED in the order top * 8 + facing * 2 + open.

// The way the player who placed it was looking.
enum class DoorFacing : uint8_t { North, East, South, West }; // -Z, +X, +Z, -X

constexpr float kDoorThickness = 3.0f / 16.0f;

inline uint8_t doorIndex(BlockType b) {
  return static_cast<uint8_t>(b) - static_cast<uint8_t>(BlockType::DOOR_BOTTOM_N_CLOSED);
}

inline bool isDoor(BlockType b) {
  return b >= BlockType::DOOR_BOTTOM_N_CLOSED && b <= BlockType::DOOR_TOP_W_OPEN;
}
inline bool doorTop(BlockType b) { return doorIndex(b) >= 8; }
inline bool doorOpen(BlockType b) { return doorIndex(b) % 2 == 1; }
inline DoorFacing doorFacing(BlockType b) {
  return static_cast<DoorFacing>((doorIndex(b) / 2) % 4);
}

inline BlockType doorBlock(bool top, DoorFacing facing, bool open) {
  return static_cast<BlockType>(static_cast<uint8_t>(BlockType::DOOR_BOTTOM_N_CLOSED) +
                                (top ? 8 : 0) + static_cast<uint8_t>(facing) * 2 +
                                (open ? 1 : 0));
}

// The same door half, swung the other way.
inline BlockType toggled(BlockType b) { return doorBlock(doorTop(b), doorFacing(b), !doorOpen(b)); }

// The facing for a player whose camera yaw (degrees; -90 looks along -Z)
// is `yaw`: whichever way they look most.
inline DoorFacing doorFacingForYaw(float yaw) {
  glm::vec2 front(glm::cos(glm::radians(yaw)), glm::sin(glm::radians(yaw)));
  if (glm::abs(front.x) > glm::abs(front.y))
    return front.x > 0 ? DoorFacing::East : DoorFacing::West;
  return front.y > 0 ? DoorFacing::South : DoorFacing::North;
}

struct DoorBox {
  glm::vec3 lo, hi; // in cell space, 0..1
};

// The panel: closed, across the cell on the side nearest whoever placed it;
// open, swung back along their left-hand edge.
inline DoorBox doorPanel(BlockType b) {
  constexpr float t = kDoorThickness;
  const bool open = doorOpen(b);
  switch (doorFacing(b)) {
    case DoorFacing::North:
      return open ? DoorBox{{0, 0, 0}, {t, 1, 1}} : DoorBox{{0, 0, 1 - t}, {1, 1, 1}};
    case DoorFacing::South:
      return open ? DoorBox{{1 - t, 0, 0}, {1, 1, 1}} : DoorBox{{0, 0, 0}, {1, 1, t}};
    case DoorFacing::East:
      return open ? DoorBox{{0, 0, 0}, {1, 1, t}} : DoorBox{{0, 0, 0}, {t, 1, 1}};
    case DoorFacing::West:
    default:
      return open ? DoorBox{{0, 0, 1 - t}, {1, 1, 1}} : DoorBox{{1 - t, 0, 0}, {1, 1, 1}};
  }
}
