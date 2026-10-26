#include "physics/ray.hpp"
#include <algorithm>
#include <limits>

std::optional<float> rayAabb(glm::vec3 origin, glm::vec3 dir, glm::vec3 box_min,
                             glm::vec3 box_max) {
  // Slab method: intersect the ray's [t_near, t_far] with each axis' slab.
  float t_near = 0.0f;
  float t_far = std::numeric_limits<float>::max();
  for (int i = 0; i < 3; ++i) {
    if (std::abs(dir[i]) < 1e-8f) {
      if (origin[i] < box_min[i] || origin[i] > box_max[i]) return std::nullopt;
      continue;
    }
    float t1 = (box_min[i] - origin[i]) / dir[i];
    float t2 = (box_max[i] - origin[i]) / dir[i];
    if (t1 > t2) std::swap(t1, t2);
    t_near = std::max(t_near, t1);
    t_far = std::min(t_far, t2);
    if (t_near > t_far) return std::nullopt;
  }
  return t_near;
}
