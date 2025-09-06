#pragma once

#include "vertex.hpp"
#include <array>

/**
 * cube.hpp
 *
 * Provides static cube geometry for voxel rendering.
 *
 */
namespace Cube {
// 6 faces, each face has 4 vertices
extern const std::array<Vertex, 4> faceVertices[6];

// 6 faces, each face has 6 indices (two triangles)
extern const std::array<unsigned int, 6> faceIndices[6];
} // namespace Cube
