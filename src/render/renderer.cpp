#include "render/renderer.hpp"
#include "chunk/chunk.hpp"
#include "core/settings.hpp"
#include "render/texture_manager.hpp"
#include <cmath>
#include <climits>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

Renderer::Renderer() : texture_manager(new TextureManager) {}

Renderer::~Renderer() {
  delete block_shader;
  delete sky_shader;
  delete cloud_shader;
  delete shadow_shader;
  delete texture_manager;
  if (sky_vao) glDeleteVertexArrays(1, &sky_vao);
  if (sky_vbo) glDeleteBuffers(1, &sky_vbo);
  if (cloud_vao) glDeleteVertexArrays(1, &cloud_vao);
  if (cloud_vbo) glDeleteBuffers(1, &cloud_vbo);
  if (cloud_ebo) glDeleteBuffers(1, &cloud_ebo);
  if (shadow_fbo) glDeleteFramebuffers(1, &shadow_fbo);
  if (shadow_depth_tex) glDeleteTextures(1, &shadow_depth_tex);
}

void Renderer::init() {
  block_shader =
      new Shader("assets/shaders/block.vert", "assets/shaders/block.frag");
  sky_shader =
      new Shader("assets/shaders/sky.vert", "assets/shaders/sky.frag");
  cloud_shader =
      new Shader("assets/shaders/cloud.vert", "assets/shaders/cloud.frag");
  shadow_shader =
      new Shader("assets/shaders/shadow_depth.vert", "assets/shaders/shadow_depth.frag");
  texture_manager->loadAtlas("res/block_atlas.png");
  initSkybox();
  initCloudBuffers();
  initShadowMap();
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

// ─── Shadow map initialisation ──────────────────────────────────────────────

void Renderer::initShadowMap() {
  glGenFramebuffers(1, &shadow_fbo);
  glGenTextures(1, &shadow_depth_tex);

  glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
               kShadowMapSize, kShadowMapSize, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  // Areas outside the shadow map should be lit (border depth = 1.0)
  float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  // Enable hardware shadow comparison (sampler2DShadow)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         GL_TEXTURE_2D, shadow_depth_tex, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ─── Light-space matrix computation ────────────────────────────────────────

glm::mat4 Renderer::computeLightSpaceMatrix(const glm::vec3 &sunDir,
                                             const glm::vec3 &cameraPos) const {
  float halfSize = g_settings.shadow_distance;

  // ── Build a stable light-view matrix ────────────────────────────────────
  // sunDir points toward the sun.  Light rays travel in the opposite
  // direction (-sunDir).  In OpenGL's right-handed view convention the
  // camera looks along its -Z axis, so the view-space +Z must point
  // toward the sun (i.e. lightZ = +sunDir).
  glm::vec3 lightZ = sunDir;   // +Z = toward sun (behind the "camera")

  glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
  if (std::abs(glm::dot(lightZ, worldUp)) > 0.99f) {
    worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
  }

  glm::vec3 lightX = glm::normalize(glm::cross(worldUp, lightZ));
  glm::vec3 lightY = glm::cross(lightZ, lightX);

  // Rotation-only view matrix (no translation — axes are fixed for a given
  // sun direction regardless of where the camera is).
  // Rows of a view matrix are the camera's basis vectors; for an
  // orthonormal basis the inverse (world→view) is the transpose.
  glm::mat4 lightView(1.0f);
  lightView[0] = glm::vec4(lightX, 0.0f);
  lightView[1] = glm::vec4(lightY, 0.0f);
  lightView[2] = glm::vec4(lightZ, 0.0f);
  lightView = glm::transpose(lightView);

  // ── Centre the ortho frustum on the camera, snapped to texels ──────────
  glm::vec3 camLS = glm::vec3(lightView * glm::vec4(cameraPos, 1.0f));

  // Snap X/Y to shadow-map texel grid so the frustum moves in discrete steps
  float texelSize = (2.0f * halfSize) / static_cast<float>(kShadowMapSize);
  float snappedX = std::floor(camLS.x / texelSize) * texelSize;
  float snappedY = std::floor(camLS.y / texelSize) * texelSize;

  // Near/far along the light's -Z (the look direction = toward the scene).
  // camLS.z is the camera's position along lightZ (toward the sun).
  // Scene geometry is at smaller Z values (in front of the light camera),
  // so near = -camLS.z - depth, far = -camLS.z + depth covers the range.
  glm::mat4 lightProj = glm::ortho(
      snappedX - halfSize, snappedX + halfSize,
      snappedY - halfSize, snappedY + halfSize,
      -camLS.z - 300.0f,   -camLS.z + 300.0f);

  return lightProj * lightView;
}

// ─── Shadow depth pass ─────────────────────────────────────────────────────

void Renderer::shadowPass(
    const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks,
    const glm::vec3 &cameraPos, float timeOfDay,
    int viewportWidth, int viewportHeight)
{
  if (!g_settings.shadows_enabled) return;

  // Compute sun direction (same formula as drawChunks)
  float angle = (timeOfDay - 0.25f) * 2.0f * glm::pi<float>();
  float sinA  = std::sin(angle);
  float cosA  = std::cos(angle);
  glm::vec3 sunDir = glm::normalize(glm::vec3(cosA * 0.6f, sinA, 0.3f));

  // Don't render shadows when sun is below the horizon
  if (sinA < -0.05f) return;

  light_space_matrix = computeLightSpaceMatrix(sunDir, cameraPos);

  // Bind shadow FBO and render depth
  glViewport(0, 0, kShadowMapSize, kShadowMapSize);
  glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
  glClear(GL_DEPTH_BUFFER_BIT);

  // Use polygon offset to push depth values slightly deeper, preventing
  // shadow acne while keeping front-face geometry in the shadow map so
  // shadows connect properly at block bases (no gap / shimmering slivers).
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.1f, 4.0f);

  shadow_shader->use();
  shadow_shader->setMat4("uLightSpaceMatrix", light_space_matrix);

  for (auto &[key, chunk] : chunks) {
    if (!chunk) continue;
    chunk->getMesh().renderOpaque();
  }

  // Restore state
  glDisable(GL_POLYGON_OFFSET_FILL);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, viewportWidth, viewportHeight);
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

// ─── Cloud mesh (per-cell slab geometry) ────────────────────────────────────

// CPU-side noise matching the shader's old algorithm so the shapes are
// identical.  We only need it here now; the shader just receives geometry.

namespace cloud_noise {

static float hash2d(float x, float y) {
  return std::fmod(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f, 1.0f);
}

static float valueNoise(float px, float py) {
  float ix = std::floor(px), iy = std::floor(py);
  float fx = px - ix,        fy = py - iy;
  float a = hash2d(ix,       iy);
  float b = hash2d(ix + 1.f, iy);
  float c = hash2d(ix,       iy + 1.f);
  float d = hash2d(ix + 1.f, iy + 1.f);
  float ux = fx * fx * (3.f - 2.f * fx);
  float uy = fy * fy * (3.f - 2.f * fy);
  return (a * (1-ux) + b * ux) * (1-uy) + (c * (1-ux) + d * ux) * uy;
}

static float fbm(float px, float py) {
  float v = 0.f, amp = 0.5f, freq = 1.f;
  for (int i = 0; i < 4; ++i) {
    v += amp * valueNoise(px * freq, py * freq);
    freq *= 2.f;
    amp  *= 0.5f;
  }
  return v;
}

static bool isCloud(float gx, float gy) {
  return fbm(gx * 0.08f, gy * 0.08f) >= 0.42f;
}

} // namespace cloud_noise

void Renderer::initCloudBuffers() {
  glGenVertexArrays(1, &cloud_vao);
  glGenBuffers(1, &cloud_vbo);
  glGenBuffers(1, &cloud_ebo);

  glBindVertexArray(cloud_vao);

  glBindBuffer(GL_ARRAY_BUFFER, cloud_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cloud_ebo);

  // vertex layout: pos (3 floats) + normal (3 floats) = 6 floats = 24 bytes
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));

  glBindVertexArray(0);
}

// Helper: push 4 verts + 6 indices for one quad face
static void emitQuad(std::vector<float> &verts,
                     std::vector<unsigned int> &indices,
                     const glm::vec3 &a, const glm::vec3 &b,
                     const glm::vec3 &c, const glm::vec3 &d,
                     const glm::vec3 &n)
{
  unsigned int base = static_cast<unsigned int>(verts.size() / 6);
  auto push = [&](const glm::vec3 &p) {
    verts.push_back(p.x); verts.push_back(p.y); verts.push_back(p.z);
    verts.push_back(n.x); verts.push_back(n.y); verts.push_back(n.z);
  };
  push(a); push(b); push(c); push(d);
  indices.push_back(base);     indices.push_back(base + 1);
  indices.push_back(base + 2); indices.push_back(base + 2);
  indices.push_back(base + 3); indices.push_back(base);
}

void Renderer::rebuildCloudMesh(const glm::vec3 &cameraPos, float cloudTime) {
  constexpr float kCell  = 12.0f;  // cloud cell size in world units
  constexpr float kDepth = 4.0f;   // slab thickness

  const float drift  = cloudTime * 0.8f;
  const float yTop   = g_settings.cloud_height;
  const float yBot   = yTop - kDepth;
  const float radius = g_settings.fog_end * 1.2f;

  // Camera's grid cell (accounts for drift so cells are stable in world space)
  int camCX = static_cast<int>(std::floor((cameraPos.x + drift) / kCell));
  int camCZ = static_cast<int>(std::floor(cameraPos.z / kCell));
  int range = static_cast<int>(std::ceil(radius / kCell));

  // Rebuild every frame because clouds drift continuously.
  // The mesh gen is cheap (~2k cells) so this is fine.

  std::vector<float>        verts;
  std::vector<unsigned int> indices;
  // Reserve rough estimate: ~40% of cells are cloud, ~3 faces avg each
  size_t estCells = static_cast<size_t>((2*range+1) * (2*range+1));
  verts.reserve(estCells * 4 * 6 * 2);   // 4 verts × 6 floats × ~2 faces
  indices.reserve(estCells * 6 * 2);      // 6 indices per face × ~2 faces

  for (int gz = camCZ - range; gz <= camCZ + range; ++gz) {
    for (int gx = camCX - range; gx <= camCX + range; ++gx) {
      if (!cloud_noise::isCloud(static_cast<float>(gx),
                                 static_cast<float>(gz)))
        continue;

      // World-space bounds of this cell (undo drift for X)
      float wx0 = gx * kCell - drift;
      float wx1 = wx0 + kCell;
      float wz0 = gz * kCell;
      float wz1 = wz0 + kCell;

      // Distance check (rough, centre of cell)
      float cdx = (wx0 + wx1) * 0.5f - cameraPos.x;
      float cdz = (wz0 + wz1) * 0.5f - cameraPos.z;
      if (cdx*cdx + cdz*cdz > radius*radius) continue;

      // ── Top face (+Y) — always emit ─────────────────────────────────
      // Extend top quads by a tiny overlap to prevent hairline seams
      constexpr float e = 0.01f;
      emitQuad(verts, indices,
               {wx0 - e, yTop, wz0 - e}, {wx1 + e, yTop, wz0 - e},
               {wx1 + e, yTop, wz1 + e}, {wx0 - e, yTop, wz1 + e},
               {0, 1, 0});

      // ── Bottom face (-Y) — always emit ──────────────────────────────
      emitQuad(verts, indices,
               {wx0 - e, yBot, wz1 + e}, {wx1 + e, yBot, wz1 + e},
               {wx1 + e, yBot, wz0 - e}, {wx0 - e, yBot, wz0 - e},
               {0, -1, 0});

      // ── Side faces — only if neighbor is air ────────────────────────
      // -X
      if (!cloud_noise::isCloud(static_cast<float>(gx - 1),
                                 static_cast<float>(gz)))
        emitQuad(verts, indices,
                 {wx0, yBot, wz1}, {wx0, yBot, wz0},
                 {wx0, yTop, wz0}, {wx0, yTop, wz1},
                 {-1, 0, 0});
      // +X
      if (!cloud_noise::isCloud(static_cast<float>(gx + 1),
                                 static_cast<float>(gz)))
        emitQuad(verts, indices,
                 {wx1, yBot, wz0}, {wx1, yBot, wz1},
                 {wx1, yTop, wz1}, {wx1, yTop, wz0},
                 {1, 0, 0});
      // -Z
      if (!cloud_noise::isCloud(static_cast<float>(gx),
                                 static_cast<float>(gz - 1)))
        emitQuad(verts, indices,
                 {wx1, yBot, wz0}, {wx0, yBot, wz0},
                 {wx0, yTop, wz0}, {wx1, yTop, wz0},
                 {0, 0, -1});
      // +Z
      if (!cloud_noise::isCloud(static_cast<float>(gx),
                                 static_cast<float>(gz + 1)))
        emitQuad(verts, indices,
                 {wx0, yBot, wz1}, {wx1, yBot, wz1},
                 {wx1, yTop, wz1}, {wx0, yTop, wz1},
                 {0, 0, 1});
    }
  }

  cloud_index_count = static_cast<int>(indices.size());
  if (cloud_index_count == 0) return;

  // Upload – grow buffers if needed, otherwise sub-data
  size_t vbytes = verts.size()   * sizeof(float);
  size_t ibytes = indices.size() * sizeof(unsigned int);

  glBindBuffer(GL_ARRAY_BUFFER, cloud_vbo);
  if (vbytes > cloud_vbo_capacity) {
    glBufferData(GL_ARRAY_BUFFER, vbytes, verts.data(), GL_DYNAMIC_DRAW);
    cloud_vbo_capacity = vbytes;
  } else {
    glBufferSubData(GL_ARRAY_BUFFER, 0, vbytes, verts.data());
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cloud_ebo);
  if (ibytes > cloud_ebo_capacity) {
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibytes, indices.data(), GL_DYNAMIC_DRAW);
    cloud_ebo_capacity = ibytes;
  } else {
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, ibytes, indices.data());
  }
}

// ─── Draw clouds ────────────────────────────────────────────────────────────
void Renderer::drawClouds(const glm::mat4 &view, const glm::mat4 &projection,
                          const glm::vec3 &cameraPos, float timeOfDay,
                          float cloudTime)
{
  if (!g_settings.clouds_enabled) return;

  float effectiveTime = cloudTime * g_settings.cloud_speed;
  rebuildCloudMesh(cameraPos, effectiveTime);
  if (cloud_index_count == 0) return;

  // Disable face culling so all sides are visible regardless of view angle
  glDisable(GL_CULL_FACE);

  cloud_shader->use();
  cloud_shader->setMat4("uView", view);
  cloud_shader->setMat4("uProjection", projection);
  cloud_shader->setFloat("uTimeOfDay", timeOfDay);
  cloud_shader->setVec3("uCameraPos", cameraPos);
  cloud_shader->setFloat("uFogEnd", g_settings.fog_end);

  glBindVertexArray(cloud_vao);
  glDrawElements(GL_TRIANGLES, cloud_index_count, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);

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

  // Bind shadow map to texture unit 1
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
  block_shader->setInt("uShadowMap", 1);
  block_shader->setMat4("uLightSpaceMatrix", light_space_matrix);
  block_shader->setBool("uShadowsEnabled", g_settings.shadows_enabled);
  glActiveTexture(GL_TEXTURE0);

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
