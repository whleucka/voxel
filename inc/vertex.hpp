#pragma once
#include <glm/glm.hpp>

/*
 * vertex.hpp
 *
 * Defines the Vertex struct used across cube, block, and mesh code.
 * 
 */
struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texCoord;
};
