#pragma once

#include <glm/glm.hpp>

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    AABB() : min(0.0f), max(0.0f) {}
    AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

    // Function to check if AABB intersects with a frustum (defined by 6 planes)
    bool intersectsFrustum(const glm::vec4 planes[6]) const;
};
