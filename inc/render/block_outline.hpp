#pragma once

#include "render/shader.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <vector>

class World;

// A thin dark frame round the block the player is aiming at: its 12 edges
// drawn as slim bars, so the frame is the same thickness on every driver
// (core-profile GL only promises 1 px lines).
class BlockOutline {
public:
  static constexpr float kThickness = 0.02f; // bar width, in blocks
  static constexpr float kInflate = 0.002f;  // sits just off the faces: no z-fighting

  BlockOutline() = default;
  ~BlockOutline();
  BlockOutline(const BlockOutline &) = delete;
  BlockOutline &operator=(const BlockOutline &) = delete;

  bool init(); // needs a GL context
  void shutdown();
  void draw(glm::ivec3 block, const glm::mat4 &view, const glm::mat4 &projection) const;

  // The block to frame for an eye looking along `dir`: the first one the
  // aim hits within reach. Never air or water (the aim passes through both).
  static std::optional<glm::ivec3> target(const World &world, glm::vec3 eye, glm::vec3 dir);

  // The frame for the block at the origin: triangles, as x y z r g b.
  static std::vector<float> mesh();

private:
  std::unique_ptr<Shader> shader;
  GLuint vao = 0, vbo = 0;
  GLsizei vertex_count = 0;
};
