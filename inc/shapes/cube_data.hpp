#pragma once

#include "shapes/cube_vertex.hpp"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace CubeData {
inline constexpr std::array<CubeVertex, 8> vertices{{
    {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},   // back top-right
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},  // back bottom-right
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}}, // back bottom-left
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // back top-left
    {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},    // front top-right
    {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},   // front bottom-right
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},  // front bottom-left
    {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},   // front top-left
}};

inline constexpr std::array<uint32_t, 36> indices{{
    // Front (+Z): 4,5,6,7
    4, 6, 5, 
    4, 7, 6,
    // Back (-Z): 0,1,2,3
    0, 1, 2,
    0, 2, 3,
    // Left (-X): 3,2,6,7
    3, 2, 6,
    3, 6, 7,
    // Right (+X): 0,1,5,4
    1, 0, 5,
    5, 0, 4,
    // Bottom (-Y): 1,2,6,5
    2, 1, 6,
    6, 1, 5,
    // Top (+Y): 0,3,7,4
    0, 3, 7,
    0, 7, 4,
}};
} // namespace CubeData
