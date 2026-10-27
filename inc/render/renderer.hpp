#pragma once

#include "chunk/chunk.hpp"
#include "render/shader.hpp"
#include "render/texture_manager.hpp"
#include <memory>
#include <vector>
#include "robin_hood/robin_hood.h"

// Per-frame culling stats for the debug panel. Written during render,
// read on the same (main) thread by the ImGui code — no sync needed.
struct RenderStats {
  int chunks_drawn = 0;         // opaque chunks submitted in the main pass
  int shadow_chunks_drawn = 0;  // chunks submitted into the shadow map
  int shadow_chunks_total = 0;  // chunks the shadow pass considered
};

// Sun, ambient and fog for a time of day; shared by every world-space pass
// (blocks, entities) so they're lit the same way.
struct SceneLighting {
  glm::vec3 sun_dir{0.0f, 1.0f, 0.0f};
  glm::vec3 sun_color{1.0f};
  glm::vec3 ambient{0.3f};
  glm::vec3 fog_color{0.5f};
  glm::vec3 water_fog_color{0.1f, 0.3f, 0.5f};
};

class Renderer {
public:
  Renderer();
  ~Renderer();

  const RenderStats &getStats() const { return stats; }

  void init();
  void drawSky(const glm::mat4 &view, const glm::mat4 &projection, float timeOfDay);
  void drawClouds(const glm::mat4 &view, const glm::mat4 &projection,
                  const glm::vec3 &cameraPos, float timeOfDay, float cloudTime);
  // Solid blocks, then (after anything else that belongs under the water,
  // like entities) the blended water and glass.
  void drawOpaqueChunks(const std::vector<Chunk *> &chunks,
                        const glm::mat4 &view, const glm::mat4 &projection,
                        const glm::vec3 &cameraPos, bool underwater, float timeOfDay);
  void drawTransparentChunks(const std::vector<Chunk *> &chunks,
                             const glm::mat4 &view, const glm::mat4 &projection,
                             const glm::vec3 &cameraPos, bool underwater, float timeOfDay);

  // Shadow map pass: render depth from the sun's POV
  void shadowPass(const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks,
                  const glm::vec3 &cameraPos, float timeOfDay,
                  int viewportWidth, int viewportHeight);

  // Compute sky/fog colour from time-of-day (used for glClearColor)
  static glm::vec3 skyColor(float timeOfDay);
  static SceneLighting sceneLighting(float timeOfDay);
  TextureManager &getTextureManager() { return *texture_manager; }

private:
  void useBlockShader(const glm::mat4 &view, const glm::mat4 &projection,
                      const glm::vec3 &cameraPos, bool underwater, float timeOfDay);
  void initSkybox();
  void initCloudBuffers();
  void initShadowMap();
  void resizeShadowMap(int size);   // re-allocate depth texture at new resolution
  // Builds the cloud volume in *cloud-grid space* (drift excluded). The drift
  // is applied at draw time via a model-matrix translation, so the mesh only
  // has to be rebuilt when the camera crosses a cell boundary.
  void rebuildCloudMesh(int camCX, int camCZ);

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
  // Settings the current cloud mesh was built with — a live change in the debug
  // panel has to force a rebuild even if the camera hasn't crossed a cell.
  float   cloud_last_height = -1.0f;
  int     cloud_last_thickness = -1;
  float   cloud_last_fog_end = -1.0f;   // drives the mesh radius

  // Shadow map
  GLuint  shadow_fbo = 0;
  GLuint  shadow_depth_tex = 0;
  glm::mat4 light_space_matrix{1.0f};
  int shadow_map_size = 2048;   // current allocated resolution

  RenderStats stats;

  TextureManager *texture_manager = nullptr;
};
