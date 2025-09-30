#pragma once

#include <vector>

#include "camera.hpp"
#include "chunk.hpp"
#include "shader.hpp"
#include "wire_cube.hpp"
#include "cloud_manager.hpp"
#include <glm/glm.hpp>

class Engine;

class Renderer {
public:
  Renderer();
  ~Renderer();

  void init();
  void draw(const std::vector<Chunk *> &chunks, const Camera &camera,
            int screen_width, int screen_height, float time_fraction);
  void drawHighlight(const Camera &camera, const glm::vec3 &block_pos);
  void drawClouds(const CloudManager &cloud_manager, const Camera &camera, int screen_width, int screen_height, float time);

private:
  Shader *block_shader;
  Shader *highlight_shader;
  Shader *cloud_shader;
  WireCube *wire_cube;

  void bindCommonUniforms(const Camera &camera);
  void drawOpaque(const Chunk &chunk) const;
  void drawTransparent(const Chunk &chunk) const;
};
