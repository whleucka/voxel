#pragma once

#include "shader.hpp"
#include <glm/glm.hpp>

/**
 * render_ctx.hpp
 *
 * A helper for the world render
 *
 */
struct renderCtx {
  Shader &blockShader;
  const glm::mat4 &view;
  const glm::mat4 &proj;
};
