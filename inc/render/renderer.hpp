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
  void drawSky(const glm::mat4 &view, const glm::mat4 &projection, float timeOfDay);
  void drawChunks(const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks,
                  const glm::mat4 &view, const glm::mat4 &projection,
                  const glm::vec3 &cameraPos, bool underwater, float timeOfDay);

  // Compute sky/fog colour from time-of-day (used for glClearColor)
  static glm::vec3 skyColor(float timeOfDay);
  TextureManager &getTextureManager() { return *texture_manager; }

private:
  void initSkybox();

  Shader *block_shader = nullptr;
  Shader *sky_shader   = nullptr;
  GLuint  sky_vao = 0;
  GLuint  sky_vbo = 0;
  TextureManager *texture_manager = nullptr;
};
