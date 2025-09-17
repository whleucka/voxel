#pragma once
#include <glm/glm.hpp>

struct Vertex {
  glm::vec3 position;  // location=0
  glm::vec3 normal;    // location=1
  glm::vec2 uvLocal;   // location=2  (in blocks; e.g. {0,h},{w,h}…)
  glm::vec2 uvBase;    // location=3  (tile base (u0,v0) in atlas [0..1])
};
static_assert(sizeof(Vertex) == 40, "Vertex must be 40 bytes");
