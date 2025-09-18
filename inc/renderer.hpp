#pragma once

#include <vector>

#include "camera.hpp"
#include "chunk.hpp"
#include "shader.hpp"

class Engine;

class Renderer {
public:
  Renderer();
  ~Renderer();

  void init();
  void draw(const std::vector<Chunk *> &chunks, const Camera &camera,
            int screen_width, int screen_height);

private:
  Shader *block_shader;

  void bindCommonUniforms(const Camera &camera);
  void drawOpaque(const Chunk &chunk) const;
  void drawTransparent(const Chunk &chunk) const;
};
