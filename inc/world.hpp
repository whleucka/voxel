#pragma once

#include "chunk.hpp"
#include "render_ctx.hpp"

class World {
public:
  World(const Texture &atlas);
  ~World();
  void update(float dt);
  void draw(renderCtx &ctx);

private:
  Chunk *m_chunk = nullptr;
};