#pragma once

#include "block.hpp"
#include "render_ctx.hpp"

class World {
public:
  World(unsigned int w, unsigned int l, float rd, const Texture &atlas)
      : width(w), length(l), renderDistance(rd) {
    testBlock = new Block(BlockType::GRASS, glm::vec3(0.0f), 16);
    testBlock->mesh.textures.clear();
    testBlock->mesh.textures.push_back(atlas);
  }
  ~World() {
    delete testBlock;
    testBlock = nullptr;
  }
  void update(float dt);
  void draw(renderCtx &ctx);

private:
  unsigned int width, length;
  float renderDistance;
  Block *testBlock = nullptr;
};
