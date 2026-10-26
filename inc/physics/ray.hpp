#pragma once

#include <glm/glm.hpp>
#include <optional>

// Distance along a ray (origin + t * dir, t >= 0) to where it enters an
// axis-aligned box, or nothing if it misses. A ray starting inside the box
// hits at t = 0. dir needn't be normalised; t is in units of dir.
std::optional<float> rayAabb(glm::vec3 origin, glm::vec3 dir, glm::vec3 box_min,
                             glm::vec3 box_max);
