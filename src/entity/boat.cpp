#include "entity/boat.hpp"
#include "block/block_data.hpp"
#include "entity/entity_type.hpp"
#include "physics/physics_body.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

namespace {

bool loaded(const World &world, glm::vec3 p) {
  int cx = static_cast<int>(std::floor(p.x / kChunkWidth));
  int cz = static_cast<int>(std::floor(p.z / kChunkDepth));
  return world.getChunk(cx, cz) != nullptr;
}

// Moves `from` toward `to` (degrees) by at most `step`, the short way round.
float turnToward(float from, float to, float step) {
  float d = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
  return from + std::clamp(d, -step, step);
}

float approach(float v, float target, float step) {
  return v < target ? std::min(target, v + step) : std::max(target, v - step);
}

PhysicsBody bodyAt(glm::vec3 pos) {
  const EntityTypeDef &def = entityType(EntityKind::Boat);
  PhysicsBody body;
  body.position = pos;
  body.half_width = def.half_width;
  body.height = def.height;
  return body;
}

void step(BoatState &boat, const BoatInput &in, const World &world, float dt) {
  std::optional<float> surface = waterSurfaceAt(world, boat.position);
  const bool floating = surface && boat.position.y <= floatHeight(*surface) + 0.01f;

  if (floating) {
    if (in.forward) {
      boat.yaw = turnToward(boat.yaw, in.look_yaw, kBoatTurnSpeed * dt);
      boat.speed = std::min(kBoatMaxSpeed, boat.speed + kBoatAccel * dt);
    } else if (in.back) {
      boat.speed = std::max(-kBoatReverseSpeed, boat.speed - kBoatAccel * dt);
    } else {
      boat.speed = approach(boat.speed, 0.0f, kBoatDrag * dt);
    }
    boat.position.y = floatHeight(*surface);
    boat.fall_speed = 0.0f;
  } else {
    // Beached or in the air: no power, and the ground stops it quickly.
    boat.speed = approach(boat.speed, 0.0f, kBoatDrag * 4.0f * dt);
    boat.fall_speed = std::max(kTerminalVelocity, boat.fall_speed + kGravityForce * dt);
  }

  PhysicsBody body = bodyAt(boat.position);
  const glm::vec3 ahead = boatForward(boat.yaw) * boat.speed;
  body.velocity = {ahead.x, floating ? 0.0f : boat.fall_speed, ahead.z};
  body.glide(dt, &world);

  // Bumped into something: it stops rather than scraping along.
  const float wanted = std::abs(boat.speed) * dt;
  const glm::vec2 moved(body.position.x - boat.position.x, body.position.z - boat.position.z);
  if (wanted > 1e-4f && glm::length(moved) < wanted * 0.5f) boat.speed = 0.0f;
  if (body.on_ground) boat.fall_speed = 0.0f;
  boat.position = body.position;

  // Fell into water (however deep it went): up to the top, and it floats.
  if (!floating) {
    const glm::vec3 p = boat.position;
    auto liquidAt = [&](int y) { return isLiquid(world.getBlockAt({p.x, y + 0.5f, p.z})); };
    int y = static_cast<int>(std::floor(p.y + 0.05f));
    if (liquidAt(y)) {
      while (y + 1 < kChunkHeight && liquidAt(y + 1)) ++y;
      boat.position.y = floatHeight(static_cast<float>(y + 1));
      boat.fall_speed = 0.0f;
    }
  }
}

} // namespace

glm::vec3 boatForward(float yaw) {
  float r = glm::radians(yaw);
  return {std::sin(r), 0.0f, -std::cos(r)};
}

glm::vec3 seatPosition(glm::vec3 boat, float yaw, int seat) {
  // The driver sits behind the middle, the passenger in front of it.
  float along = seat == 0 ? -kBoatSeatOffset : kBoatSeatOffset;
  return boat + boatForward(yaw) * along + glm::vec3(0.0f, kBoatSeatHeight, 0.0f);
}

std::optional<float> waterSurfaceAt(const World &world, glm::vec3 pos) {
  const float x = std::floor(pos.x) + 0.5f, z = std::floor(pos.z) + 0.5f;
  const int bottom = static_cast<int>(std::floor(pos.y));
  for (int y = bottom + 1; y >= bottom - 1; --y) {
    if (!isLiquid(world.getBlockAt({x, y + 0.5f, z}))) continue;
    if (isLiquid(world.getBlockAt({x, y + 1.5f, z}))) continue; // deeper than the top
    return static_cast<float>(y + 1);
  }
  return std::nullopt;
}

bool afloat(const World &world, glm::vec3 pos) {
  std::optional<float> s = waterSurfaceAt(world, pos);
  return s && std::abs(pos.y - floatHeight(*s)) < 0.3f;
}

void steerBoat(BoatState &boat, const BoatInput &input, const World &world, float dt) {
  if (!loaded(world, boat.position)) {
    boat.speed = 0.0f;
    boat.fall_speed = 0.0f;
    return;
  }
  // A long frame goes in short steps, so turning and bumping stay smooth.
  constexpr float kMaxStep = 0.05f;
  const int n = std::max(1, static_cast<int>(std::ceil(dt / kMaxStep)));
  for (int i = 0; i < n; ++i) step(boat, input, world, dt / static_cast<float>(n));
}

bool boatMoveAllowed(const World &world, glm::vec3 from, glm::vec3 to) {
  if (!std::isfinite(to.x) || !std::isfinite(to.y) || !std::isfinite(to.z)) return false;
  const float across = glm::length(glm::vec2(to.x - from.x, to.z - from.z));
  if (across > kBoatMaxStep || to.y > from.y + 0.5f) return false;
  if (afloat(world, to)) return true;
  // Off the water it can only settle (fall, or come to rest).
  return across < 0.05f;
}
