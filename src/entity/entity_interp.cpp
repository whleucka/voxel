#include "entity/entity_interp.hpp"
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
    }
    out.push_back(v);
  }
  return out;
}
