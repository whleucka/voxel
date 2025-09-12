#pragma once

#include "chunk.hpp"
#include "render_ctx.hpp"
#include "texture.hpp"
#include <vector>

/*
 * world.hpp
 *
 * 3d minecraft-like voxel game world
 * 
 */
class World {
public:
  World(Texture &atlas);
  ~World();
  void update(float);
  void draw(renderCtx &ctx);
  BlockType getBlock(int x, int y, int z);
  int getChunkCount() const;
  void loadChunk(glm::vec2 pos);
  void unloadChunk();
  Texture& atlas;

private:
  std::vector<Chunk *> chunks;
  int width = 10;
  int length = 10;
};
