#pragma once

#include <glm/glm.hpp>

struct BlockVertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 uv;         // baseUV in block space: (0..width, 0..height)
  glm::vec2 tileOffset; // atlas origin of the tile (u0,v0)
  glm::vec2 tileSpan;   // size of one tile in atlas (du,dv) after padding
};
