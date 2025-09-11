#pragma once

#include "chunk.hpp"
#include "render_ctx.hpp"

/**
 * world.hpp
 *
 * A 3d world for my voxel game
 *
 */
class World {
public:
  World(const Texture &atlas);
  ~World();
  void update(float dt);
  void draw(renderCtx &ctx);

private:
  Chunk *chunk = nullptr;
};
