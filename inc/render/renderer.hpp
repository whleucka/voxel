#pragma once

#include "chunk/chunk.hpp"
#include "render/shader.hpp"
#include "render/texture_manager.hpp"
#include <memory>
#include "robin_hood/robin_hood.h"

class Renderer {
public:
  Renderer();
  ~Renderer();

  void init();
  void drawChunks(const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks, const glm::mat4 &view, const glm::mat4 &projection);
  TextureManager &getTextureManager() { return *texture_manager; }

private:
  Shader *block_shader = nullptr;
  TextureManager *texture_manager = nullptr;
};
