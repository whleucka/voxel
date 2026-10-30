#pragma once

#include "block/door.hpp"
#include "render/shader.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <vector>

class World;

// A thin dark frame round the block the player is aiming at: its 12 edges
// drawn as slim bars, so the frame is the same thickness on every driver
// (core-profile GL only promises 1 px lines). It hugs the block's shape: a
// slab's half, a door's panel (both halves), a whole bed, a fence's post and
// rails, a ladder against its wall.
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
  // Frames `shape` (boxes(), in cell space) at `block`.
  void draw(glm::ivec3 block, const std::vector<DoorBox> &shape, const glm::mat4 &view,
            const glm::mat4 &projection);

  // The block to frame for an eye looking along `dir`: the first one the
  // aim hits within `reach`. Never air or water (the aim passes through
  // both). A creature or a boat in front shortens the reach to it, so the
  // block behind isn't framed.
  static std::optional<glm::ivec3> target(const World &world, glm::vec3 eye, glm::vec3 dir,
                                          float reach);

  // The boxes to frame for the block at `pos`, in its cell's space (they may
  // reach into the next cell: a door's other half, a bed's other end).
  // Usually one; a stair's two steps.
  static std::vector<DoorBox> boxes(const World &world, glm::ivec3 pos);

  // The frame round `shape` (default: the whole cell): triangles, as
  // x y z r g b.
  static std::vector<float> mesh(const std::vector<DoorBox> &shape = {{{0, 0, 0}, {1, 1, 1}}});

private:
  std::unique_ptr<Shader> shader;
  GLuint vao = 0, vbo = 0;
  GLsizei vertex_count = 0;
  std::vector<DoorBox> uploaded; // the shape the buffer holds
};
