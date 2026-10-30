#pragma once

#include "chunk/chunk.hpp"
#include "render/gpu_timer.hpp"
#include "render/shader.hpp"
#include "render/texture_manager.hpp"
#include <memory>
#include <vector>
#include "robin_hood/robin_hood.h"

// Per-frame culling stats for the debug panel. Written during render,
// read on the same (main) thread by the ImGui code — no sync needed.
struct RenderStats {
  int chunks_drawn = 0;         // opaque chunks submitted in the main pass
  int shadow_chunks_drawn = 0;  // chunks in the shadow map, as last drawn
  int shadow_chunks_total = 0;  // chunks the shadow pass considered
  // The shadow pass on the GPU some frames ago, when a timing came in.
  std::optional<double> shadow_gpu_ms;
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

// When the shadow map has to be drawn again. Drawing it costs a good part of
// the frame, but the sun moves slowly, so a map is kept until the sun has
// turned or the camera has moved enough to show, or something that casts a
// shadow in its box has changed.
struct ShadowRefresh {
  static constexpr float kSunStep = 0.002f; // radians (the sun turns this in ~0.4 s)
  static constexpr float kMoveStep = 4.0f;  // blocks

  // Whether a map drawn now would differ enough from the one kept.
  bool due(const glm::vec3 &sun_dir, const glm::vec3 &camera, float distance,
           int map_size) const;
  // A map was drawn for these, covering `light_space`.
  void drawn(const glm::vec3 &sun_dir, const glm::vec3 &camera, float distance,
             int map_size, const glm::mat4 &light_space);
  // The map no longer stands (shadows off, the sun down): draw the next one.
  void invalidate() { valid = false; }
  // A chunk's mesh changed or went: the map is due if the chunk is in its box.
  void chunkChanged(const ChunkKey &key);

private:
  bool valid = false, dirty = false;
  glm::vec3 sun_dir{0.0f}, camera{0.0f};
  float distance = 0.0f;
  int map_size = 0;
  glm::vec4 planes[6] = {};
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

  // A chunk's mesh was uploaded or unloaded: its shadow may have changed.
  void shadowCasterChanged(const ChunkKey &key) { shadow_refresh.chunkChanged(key); }

  // Shadow map pass: render depth from the sun's POV, when it's due
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
  GpuTimer shadow_timer;
  ShadowRefresh shadow_refresh;

  TextureManager *texture_manager = nullptr;
};
