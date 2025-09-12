#pragma once

#include "chunk.hpp"
#include "render_ctx.hpp"
#include <vector>

/*
 * world.hpp
 *
 * 3d minecraft-like voxel game world
 * 
 */
class World {
public:
  World(const Texture &atlas);
  ~World();
  void update(float);
  void draw(renderCtx &ctx);
  BlockType getBlock(int x, int y, int z);

private:
  std::vector<Chunk *> chunks;
  int width = 10;
  int length = 10;
};
