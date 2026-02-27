#include "render/renderer.hpp"
#include "chunk/chunk.hpp"
#include "core/settings.hpp"
#include "render/texture_manager.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

Renderer::Renderer() : texture_manager(new TextureManager) {}

Renderer::~Renderer() {
  delete block_shader;
  delete sky_shader;
  delete texture_manager;
  if (sky_vao) glDeleteVertexArrays(1, &sky_vao);
  if (sky_vbo) glDeleteBuffers(1, &sky_vbo);
}

void Renderer::init() {
  block_shader =
      new Shader("assets/shaders/block.vert", "assets/shaders/block.frag");
  sky_shader =
      new Shader("assets/shaders/sky.vert", "assets/shaders/sky.frag");
  texture_manager->loadAtlas("res/block_atlas.png");
  initSkybox();
}

void Renderer::initSkybox() {
  // Unit cube – 36 vertices wound so faces are visible from inside.
  // The vertex position is also used as the sample direction in the shader.
  static const float kVerts[] = {
    // -Z face
    -1, 1,-1,  -1,-1,-1,   1,-1,-1,
     1,-1,-1,   1, 1,-1,  -1, 1,-1,
    // -X face
    -1,-1, 1,  -1,-1,-1,  -1, 1,-1,
    -1, 1,-1,  -1, 1, 1,  -1,-1, 1,
    // +X face
     1,-1,-1,   1,-1, 1,   1, 1, 1,
     1, 1, 1,   1, 1,-1,   1,-1,-1,
    // +Z face
    -1,-1, 1,   1,-1, 1,   1, 1, 1,
     1, 1, 1,  -1, 1, 1,  -1,-1, 1,
    // +Y face
    -1, 1,-1,   1, 1,-1,   1, 1, 1,
     1, 1, 1,  -1, 1, 1,  -1, 1,-1,
    // -Y face
    -1,-1,-1,  -1,-1, 1,   1,-1,-1,
     1,-1,-1,  -1,-1, 1,   1,-1, 1,
  };

  glGenVertexArrays(1, &sky_vao);
  glGenBuffers(1, &sky_vbo);

  glBindVertexArray(sky_vao);
  glBindBuffer(GL_ARRAY_BUFFER, sky_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kVerts), kVerts, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
  glBindVertexArray(0);
}

// ─── Draw sky ───────────────────────────────────────────────────────────────
void Renderer::drawSky(const glm::mat4 &view, const glm::mat4 &projection,
                       float timeOfDay)
{
  // Render the skybox before world geometry.
  // Depth test uses GL_LEQUAL because sky.vert writes z = w → NDC depth = 1.0,
  // and the cleared depth buffer is also 1.0. GL_LESS would reject those fragments.
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);

  sky_shader->use();
  sky_shader->setMat4("uView",       view);
  sky_shader->setMat4("uProjection", projection);
  sky_shader->setFloat("uTimeOfDay", timeOfDay);

  glBindVertexArray(sky_vao);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);

  // Restore state for world rendering
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);
}

// ─── Shared sky colour helper ───────────────────────────────────────────────
// timeOfDay: 0 = midnight, 0.25 = dawn, 0.5 = noon, 0.75 = dusk
glm::vec3 Renderer::skyColor(float timeOfDay) {
  // Sun elevation angle: -π/2 at midnight, 0 at dawn, +π/2 at noon
  float angle  = (timeOfDay - 0.25f) * 2.0f * glm::pi<float>();
  float sinA   = std::sin(angle);

  // 0→1 brightness factor (rises above the horizon with a small softening)
  float dayFactor  = glm::clamp(sinA + 0.1f, 0.0f, 1.0f);

  // Peaks sharply at dawn / dusk (sinA ≈ 0)
  float dawnFactor = std::pow(1.0f - std::abs(sinA), 4.0f)
                   * glm::clamp(sinA + 0.15f, 0.0f, 1.0f);

  const glm::vec3 kDayColor   { 0.53f,  0.81f,  0.92f  };
  const glm::vec3 kNightColor { 0.001f, 0.001f, 0.003f }; // near-black – sRGB gamma makes even 0.01 glow
  const glm::vec3 kDawnColor  { 0.85f,  0.45f,  0.20f  };

  glm::vec3 sky = glm::mix(kNightColor, kDayColor, dayFactor);
  sky           = glm::mix(sky,         kDawnColor, dawnFactor * 0.45f);
  return sky;
}

// ─── Draw chunks ────────────────────────────────────────────────────────────
void Renderer::drawChunks(
    const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks,
    const glm::mat4 &view, const glm::mat4 &projection,
    const glm::vec3 &cameraPos, bool underwater, float timeOfDay)
{
  // ── Derive lighting parameters from time-of-day ──────────────────────────
  float angle  = (timeOfDay - 0.25f) * 2.0f * glm::pi<float>();
  float sinA   = std::sin(angle);
  float cosA   = std::cos(angle);

  // Sun direction: sweeps from East (+X) at dawn, overhead (+Y) at noon, West at dusk
  glm::vec3 sunDir = glm::normalize(glm::vec3(cosA * 0.6f, sinA, 0.3f));

  float dayFactor  = glm::clamp(sinA + 0.1f, 0.0f, 1.0f);
  float dawnFactor = std::pow(1.0f - std::abs(sinA), 4.0f)
                   * glm::clamp(sinA + 0.15f, 0.0f, 1.0f);

  // Sun colour: warm white during day, orange at dawn/dusk, off at night
  glm::vec3 sunColor = glm::mix(
      glm::vec3(0.0f, 0.0f, 0.0f),    // night – no direct light
      glm::vec3(1.0f, 0.95f, 0.80f),  // midday white
      dayFactor);
  // Dawn/dusk warm tint
  sunColor = glm::mix(sunColor, glm::vec3(1.0f, 0.55f, 0.20f), dawnFactor * 0.6f);

  // Ambient: very dim moonlight at night, cool grey by day.
  // Keep night values tiny – sRGB gamma correction makes even 0.05 look ~25%
  // bright, so a "dark" night needs linear values well below 0.01.
  glm::vec3 ambientColor = glm::mix(
      glm::vec3(0.006f, 0.007f, 0.014f), // night – faint blue moonlight
      glm::vec3(0.28f,  0.30f,  0.34f),  // day
      dayFactor);

  // Fog colour matches the sky
  glm::vec3 fogColor = skyColor(timeOfDay);

  // Fog distances from settings (tweakable in debug panel)
  const float kFogStart = g_settings.fog_start;
  const float kFogEnd   = g_settings.fog_end;

  // ── Shader setup ─────────────────────────────────────────────────────────
  block_shader->use();
  texture_manager->bind(GL_TEXTURE0);
  block_shader->setInt("uTexture", 0);
  block_shader->setMat4("view", view);
  block_shader->setMat4("projection", projection);
  block_shader->setVec3("uCameraPos", cameraPos);
  block_shader->setBool("uUnderwater", underwater);

  block_shader->setVec3("uSunDir",       sunDir);
  block_shader->setVec3("uSunColor",     sunColor);
  block_shader->setVec3("uAmbientColor", ambientColor);
  block_shader->setVec3("uFogColor",     fogColor);
  block_shader->setFloat("uFogStart",         kFogStart);
  block_shader->setFloat("uFogEnd",           kFogEnd);
  block_shader->setFloat("uWaterFogDensity",  g_settings.water_fog_density);
  // Water fog colour: medium blue by day, fades to near-black at night
  // Night value kept near-black: 0.04 linear blue → ~21 % after sRGB gamma,
  // which is the source of the "glowing underwater wall" at night.
  glm::vec3 waterFogColor = glm::mix(glm::vec3(0.001f, 0.002f, 0.004f),
                                     glm::vec3(0.10f, 0.30f, 0.50f),
                                     dayFactor);
  block_shader->setVec3("uWaterFogColor", waterFogColor);

  // ── Pass 1: Opaque geometry ───────────────────────────────────────────────
  block_shader->setFloat("uAlpha", 1.0f);
  for (auto &[key, chunk] : chunks) {
    if (!chunk) continue;
    chunk->getMesh().renderOpaque();
  }

  // ── Pass 2: Transparent geometry (blended) ────────────────────────────────
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  block_shader->setFloat("uAlpha", 0.75f);
  for (auto &[key, chunk] : chunks) {
    if (!chunk) continue;
    chunk->getMesh().renderTransparent();
  }

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}
