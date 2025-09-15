#pragma once

#include "camera.hpp" // Include camera header
#include "shader.hpp"
#include <glm/glm.hpp>

/**
 * render_ctx.hpp
 *
 * A helper for the world render
 *
 */
struct renderCtx {
  Shader &block_shader;
  const glm::mat4 &view;
  const glm::mat4 &proj;
  const Camera &camera; // Add camera member
};
