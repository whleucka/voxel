#pragma once

#include "chunk/chunk.hpp"
#include "render/shader.hpp"
#include "render/texture_manager.hpp"
#include <memory>
#include <vector>
#include "robin_hood/robin_hood.h"

class Renderer {
public:
  Renderer();
  ~Renderer();

  void init();
  void drawSky(const glm::mat4 &view, const glm::mat4 &projection, float timeOfDay);
  void drawClouds(const glm::mat4 &view, const glm::mat4 &projection,
                  const glm::vec3 &cameraPos, float timeOfDay, float cloudTime);
  void drawChunks(const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks,
                  const glm::mat4 &view, const glm::mat4 &projection,
                  const glm::vec3 &cameraPos, bool underwater, float timeOfDay);

  // Shadow map pass: render depth from the sun's POV
  void shadowPass(const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks,
                  const glm::vec3 &cameraPos, float timeOfDay,
                  int viewportWidth, int viewportHeight);

  // Compute sky/fog colour from time-of-day (used for glClearColor)
  static glm::vec3 skyColor(float timeOfDay);
  TextureManager &getTextureManager() { return *texture_manager; }

private:
  void initSkybox();
  void initCloudBuffers();
  void initShadowMap();
  void rebuildCloudMesh(const glm::vec3 &cameraPos, float cloudTime);

  glm::mat4 computeLightSpaceMatrix(const glm::vec3 &sunDir,
                                     const glm::vec3 &cameraPos) const;

  Shader *block_shader  = nullptr;
  Shader *sky_shader    = nullptr;
  Shader *cloud_shader  = nullptr;
  Shader *shadow_shader = nullptr;
  GLuint  sky_vao = 0;
  GLuint  sky_vbo = 0;
  GLuint  cloud_vao = 0;
  GLuint  cloud_vbo = 0;
  GLuint  cloud_ebo = 0;
  int     cloud_index_count = 0;        // number of indices to draw
  size_t  cloud_vbo_capacity = 0;       // current GPU buffer sizes (bytes)
  size_t  cloud_ebo_capacity = 0;
  int     cloud_last_cx = INT_MIN;      // last camera cell for rebuild check
  int     cloud_last_cz = INT_MIN;
  float   cloud_last_time = -1.0f;      // last cloud time used for mesh build

  // Shadow map
  GLuint  shadow_fbo = 0;
  GLuint  shadow_depth_tex = 0;
  glm::mat4 light_space_matrix{1.0f};
  static constexpr int kShadowMapSize = 4096;

  TextureManager *texture_manager = nullptr;
};
