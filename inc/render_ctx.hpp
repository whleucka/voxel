#pragma once

#include "shader.hpp"
#include <glm/glm.hpp>

struct renderCtx {
  Shader &blockShader;
  const glm::mat4 &view;
  const glm::mat4 &proj;
};
