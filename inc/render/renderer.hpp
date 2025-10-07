#pragma once

#include "chunk/chunk.hpp"
#include "render/shader.hpp"
#include "render/texture_manager.hpp"
#include <memory>
#include <unordered_map>

class Renderer {
public:
  Renderer();
  ~Renderer();

  void init();
  void drawChunks(const std::vector<std::shared_ptr<Chunk>> &chunks, const glm::mat4 &view, const glm::mat4 &projection);
  TextureManager &getTextureManager() { return *texture_manager; }

private:
  Shader *block_shader = nullptr;
  TextureManager *texture_manager = nullptr;
};
