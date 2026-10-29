#include "entity/entity_interp.hpp"
#include "entity/boat.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

// Blend angles in degrees the short way round.
float lerpAngle(float a, float b, float t) {
  float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
  return a + d * t;
}

} // namespace

void EntityInterpolator::push(EntitySnapshot snap) {
  prev = std::move(curr);
  have_prev = true;
  curr = std::move(snap);
}

std::vector<PlayerView> EntityInterpolator::players(float alpha) const {
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  std::unordered_map<PlayerId, const PlayerView *> before;
  if (have_prev)
    for (const PlayerView &v : prev.players) before[v.id] = &v;

  std::vector<PlayerView> out;
  out.reserve(curr.players.size());
  for (const PlayerView &now : curr.players) {
    PlayerView v = now;
    auto it = before.find(now.id);
    if (it != before.end()) {
      const PlayerView &was = *it->second;
      v.position = glm::mix(was.position, now.position, alpha);
      v.yaw = lerpAngle(was.yaw, now.yaw, alpha);
      v.pitch = was.pitch + (now.pitch - was.pitch) * alpha;
      glm::vec2 step(now.position.x - was.position.x, now.position.z - was.position.z);
      v.moving = glm::length(step) > 0.01f;
    }
    out.push_back(v);
  }
  return out;
}

std::vector<EntityView> EntityInterpolator::views(float alpha) const {
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  std::unordered_map<EntityId, const EntityView *> before;
  if (have_prev)
    for (const EntityView &v : prev.entities) before[v.id] = &v;

  std::vector<EntityView> out;
  out.reserve(curr.entities.size());
  for (const EntityView &now : curr.entities) {
    EntityView v = now;
    auto it = before.find(now.id);
    if (it != before.end()) {
      const EntityView &was = *it->second;
      v.position = glm::mix(was.position, now.position, alpha);
      v.yaw = lerpAngle(was.yaw, now.yaw, alpha);
      // Walking pace is ~0.1 blocks a tick; anything over a small threshold
      // counts as moving (drives leg swing).
      glm::vec2 step(now.position.x - was.position.x, now.position.z - was.position.z);
      v.moving = glm::length(step) > 0.01f;
    }
    out.push_back(v);
  }
  return out;
}

void placeRiders(std::vector<PlayerView> &players, const std::vector<EntityView> &entities) {
  for (PlayerView &p : players) {
    if (p.riding == 0) continue;
    for (const EntityView &boat : entities)
      if (boat.id == p.riding) {
        p.position = seatPosition(boat.position, boat.yaw, p.seat);
        p.yaw = boat.yaw - 90.0f; // a boat's yaw as a camera's
        p.moving = false;
        break;
      }
  }
}
