#include "render/renderer.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
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
  shadow_map_size = g_settings.shadow_map_size;

  glGenFramebuffers(1, &shadow_fbo);
  glGenTextures(1, &shadow_depth_tex);

  glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
               shadow_map_size, shadow_map_size, 0,
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

// Re-allocate the depth texture storage at a new resolution. The texture's
// sampler params and FBO attachment persist across a glTexImage2D re-spec, so
// only the backing store is replaced. Must run on the main thread (GL context).
void Renderer::resizeShadowMap(int size) {
  if (size == shadow_map_size) return;
  shadow_map_size = size;
  glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
               shadow_map_size, shadow_map_size, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glBindTexture(GL_TEXTURE_2D, 0);
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
  float texelSize = (2.0f * halfSize) / static_cast<float>(shadow_map_size);
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

// ─── Frustum helpers (shadow-caster culling) ───────────────────────────────
// Gribb-Hartmann plane extraction — identical to Camera::getFrustumPlanes but
// usable against any view-projection matrix (here, the light-space matrix).
static void extractFrustumPlanes(const glm::mat4 &m, glm::vec4 planes[6]) {
  planes[0] = glm::vec4(m[0][3]+m[0][0], m[1][3]+m[1][0], m[2][3]+m[2][0], m[3][3]+m[3][0]); // Left
  planes[1] = glm::vec4(m[0][3]-m[0][0], m[1][3]-m[1][0], m[2][3]-m[2][0], m[3][3]-m[3][0]); // Right
  planes[2] = glm::vec4(m[0][3]+m[0][1], m[1][3]+m[1][1], m[2][3]+m[2][1], m[3][3]+m[3][1]); // Bottom
  planes[3] = glm::vec4(m[0][3]-m[0][1], m[1][3]-m[1][1], m[2][3]-m[2][1], m[3][3]-m[3][1]); // Top
  planes[4] = glm::vec4(m[0][3]+m[0][2], m[1][3]+m[1][2], m[2][3]+m[2][2], m[3][3]+m[3][2]); // Near
  planes[5] = glm::vec4(m[0][3]-m[0][2], m[1][3]-m[1][2], m[2][3]-m[2][2], m[3][3]-m[3][2]); // Far
  for (int i = 0; i < 6; i++) {
    float len = glm::length(glm::vec3(planes[i]));
    if (len > 0.0f) planes[i] /= len;
  }
}

// AABB vs frustum: false only when the box is entirely behind some plane.
static bool aabbInFrustum(const glm::vec4 planes[6], const glm::vec3 &min,
                          const glm::vec3 &max) {
  for (int i = 0; i < 6; i++) {
    const glm::vec3 n = glm::vec3(planes[i]);
    glm::vec3 positive = {(n.x > 0 ? max.x : min.x), (n.y > 0 ? max.y : min.y),
                          (n.z > 0 ? max.z : min.z)};
    if (glm::dot(n, positive) + planes[i].w < 0)
      return false;
  }
  return true;
}

// ─── Shadow depth pass ─────────────────────────────────────────────────────

void Renderer::shadowPass(
    const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks,
    const glm::vec3 &cameraPos, float timeOfDay,
    int viewportWidth, int viewportHeight)
{
  // Default to "nothing drawn" so the debug panel is correct on any early out.
  stats.shadow_chunks_total = static_cast<int>(chunks.size());
  stats.shadow_chunks_drawn = 0;

  // Apply any live resolution change from the debug panel.
  resizeShadowMap(g_settings.shadow_map_size);

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
  glViewport(0, 0, shadow_map_size, shadow_map_size);
  glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
  glClear(GL_DEPTH_BUFFER_BIT);

  // Use polygon offset to push depth values slightly deeper, preventing
  // shadow acne while keeping front-face geometry in the shadow map so
  // shadows connect properly at block bases (no gap / shimmering slivers).
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.1f, 4.0f);

  shadow_shader->use();
  shadow_shader->setMat4("uLightSpaceMatrix", light_space_matrix);

  // The depth pass samples the atlas so alpha-cutout blocks (leaves) drop their
  // see-through texels instead of casting shadow from them.
  glActiveTexture(GL_TEXTURE0);
  texture_manager->bind(GL_TEXTURE0);
  shadow_shader->setInt("uTexture", 0);

  // Cull shadow casters to the light frustum. The ortho box only covers a
  // shadow_distance-sized region around the camera, so the vast majority of
  // loaded chunks fall outside it and would otherwise be clipped after a
  // wasted draw call.
  glm::vec4 lightPlanes[6];
  extractFrustumPlanes(light_space_matrix, lightPlanes);

  for (auto &[key, chunk] : chunks) {
    if (!chunk) continue;
    glm::vec3 min = {key.x * kChunkWidth, 0.0f, key.z * kChunkDepth};
    glm::vec3 max = {(key.x + 1) * kChunkWidth,
                     static_cast<float>(kChunkHeight),
                     (key.z + 1) * kChunkDepth};
    if (!aabbInFrustum(lightPlanes, min, max)) continue;
    chunk->getMesh().renderOpaque();
    ++stats.shadow_chunks_drawn;
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

// ─── Cloud mesh (3D voxel volume) ───────────────────────────────────────────

constexpr float kCloudCell  = 12.0f;  // horizontal cell size in world units
constexpr float kCloudCellY = 6.0f;   // vertical cell size — finer, so a band
                                      // of a few layers still reads as billowy
constexpr float kCloudDrift = 0.8f;   // base drift speed (world units / sec)

namespace cloud_noise {

// Note: this is fract(), not fmod() — fmod keeps the sign, which left the old
// 2D field centred on zero. The density thresholds below assume [0,1).
static float hash3d(float x, float y, float z) {
  float v = std::sin(x * 127.1f + y * 311.7f + z * 74.7f) * 43758.5453f;
  return v - std::floor(v);
}

static float valueNoise(float px, float py, float pz) {
  float ix = std::floor(px), iy = std::floor(py), iz = std::floor(pz);
  float fx = px - ix,        fy = py - iy,        fz = pz - iz;
  float ux = fx * fx * (3.f - 2.f * fx);
  float uy = fy * fy * (3.f - 2.f * fy);
  float uz = fz * fz * (3.f - 2.f * fz);

  auto corner = [&](float dx, float dy, float dz) {
    return hash3d(ix + dx, iy + dy, iz + dz);
  };
  // Trilinear blend of the 8 lattice corners
  float x00 = corner(0,0,0) * (1-ux) + corner(1,0,0) * ux;
  float x10 = corner(0,1,0) * (1-ux) + corner(1,1,0) * ux;
  float x01 = corner(0,0,1) * (1-ux) + corner(1,0,1) * ux;
  float x11 = corner(0,1,1) * (1-ux) + corner(1,1,1) * ux;
  float y0  = x00 * (1-uy) + x10 * uy;
  float y1  = x01 * (1-uy) + x11 * uy;
  return y0 * (1-uz) + y1 * uz;
}

static float fbm(float px, float py, float pz) {
  float v = 0.f, amp = 0.5f, freq = 1.f;
  for (int i = 0; i < 4; ++i) {
    v += amp * valueNoise(px * freq, py * freq, pz * freq);
    freq *= 2.f;
    amp  *= 0.5f;
  }
  return v;
}

// Vertical envelope over the band, t = 0 at the base, 1 at the top. Full
// strength at the bottom and tapering upward: that's what gives clouds a flat
// underside with a billowing crown above it.
static float heightGradient(float t) {
  auto smoothstep = [](float e0, float e1, float x) {
    float u = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return u * u * (3.f - 2.f * u);
  };
  return 1.0f - smoothstep(0.20f, 1.0f, t);
}

// Density at a cell in the cloud grid. gy is the layer index within a band of
// `layers`. Vertical frequency runs higher than horizontal so successive layers
// differ from one another instead of extruding the same 2D blob.
//
// Height raises the *threshold* rather than scaling the density: fbm only spans
// ~0..0.8, so scaling would push the upper band mathematically out of reach and
// cut the tops off flat. Raising the bar instead lets coverage taper smoothly,
// with a few dense columns still towering into the top layer.
static bool isCloud(int gx, int gy, int gz, int layers) {
  if (gy < 0 || gy >= layers) return false;

  constexpr float kBaseThreshold = 0.50f;  // coverage at the cloud base (~40%)
  constexpr float kHeightFalloff = 0.22f;  // how fast the bar rises with height

  float t = (static_cast<float>(gy) + 0.5f) / static_cast<float>(layers);
  float d = fbm(gx * 0.08f, gy * 0.16f, gz * 0.08f);
  return d >= kBaseThreshold + (1.0f - heightGradient(t)) * kHeightFalloff;
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

void Renderer::rebuildCloudMesh(int camCX, int camCZ) {
  const int   layers = std::max(1, g_settings.cloud_thickness);
  const float yBase  = g_settings.cloud_height;
  const float radius = g_settings.fog_end * 1.2f;
  const int   range  = static_cast<int>(std::ceil(radius / kCloudCell));

  // Sample the density field once into a solid/air bitmap, with a one-cell
  // border in X/Z so face culling can look at neighbours without re-evaluating
  // the noise. Layers outside the band are air by definition, so Y needs no
  // border. Meshing then reads the bitmap instead of calling fbm ~7× per cell.
  const int dim = 2 * range + 3;             // +2 for the border, +1 for centre
  std::vector<uint8_t> solid(static_cast<size_t>(dim) * dim * layers, 0);

  auto at = [&](int lx, int ly, int lz) -> size_t {
    return (static_cast<size_t>(ly) * dim + lz) * dim + lx;
  };
  // Is the cell at local coords solid? Out of range in X/Z or Y ⇒ air.
  auto solidAt = [&](int lx, int ly, int lz) -> bool {
    if (lx < 0 || lx >= dim || lz < 0 || lz >= dim) return false;
    if (ly < 0 || ly >= layers) return false;
    return solid[at(lx, ly, lz)] != 0;
  };

  for (int ly = 0; ly < layers; ++ly)
    for (int lz = 0; lz < dim; ++lz)
      for (int lx = 0; lx < dim; ++lx) {
        int gx = camCX - range - 1 + lx;
        int gz = camCZ - range - 1 + lz;
        if (cloud_noise::isCloud(gx, ly, gz, layers))
          solid[at(lx, ly, lz)] = 1;
      }

  std::vector<float>        verts;
  std::vector<unsigned int> indices;
  size_t estCells = static_cast<size_t>(dim) * dim * layers / 4;  // ~25% solid
  verts.reserve(estCells * 4 * 6 * 2);
  indices.reserve(estCells * 6 * 2);

  // Cell centre used for the radial cull, in grid space
  const float camGX = (camCX + 0.5f) * kCloudCell;
  const float camGZ = (camCZ + 0.5f) * kCloudCell;

  for (int ly = 0; ly < layers; ++ly) {
    for (int lz = 1; lz < dim - 1; ++lz) {
      for (int lx = 1; lx < dim - 1; ++lx) {
        if (!solidAt(lx, ly, lz)) continue;

        int gx = camCX - range - 1 + lx;
        int gz = camCZ - range - 1 + lz;

        // Grid-space bounds of this cell (drift is applied by the model matrix)
        float wx0 = gx * kCloudCell,  wx1 = wx0 + kCloudCell;
        float wz0 = gz * kCloudCell,  wz1 = wz0 + kCloudCell;
        float wy0 = yBase + ly * kCloudCellY;
        float wy1 = wy0 + kCloudCellY;

        // Radial cull against the fog radius (rough, cell centre)
        float cdx = (wx0 + wx1) * 0.5f - camGX;
        float cdz = (wz0 + wz1) * 0.5f - camGZ;
        if (cdx*cdx + cdz*cdz > radius*radius) continue;

        // Tiny overlap on the horizontal faces to hide hairline seams
        constexpr float e = 0.01f;

        // +Y
        if (!solidAt(lx, ly + 1, lz))
          emitQuad(verts, indices,
                   {wx0 - e, wy1, wz0 - e}, {wx1 + e, wy1, wz0 - e},
                   {wx1 + e, wy1, wz1 + e}, {wx0 - e, wy1, wz1 + e},
                   {0, 1, 0});
        // -Y
        if (!solidAt(lx, ly - 1, lz))
          emitQuad(verts, indices,
                   {wx0 - e, wy0, wz1 + e}, {wx1 + e, wy0, wz1 + e},
                   {wx1 + e, wy0, wz0 - e}, {wx0 - e, wy0, wz0 - e},
                   {0, -1, 0});
        // -X
        if (!solidAt(lx - 1, ly, lz))
          emitQuad(verts, indices,
                   {wx0, wy0, wz1}, {wx0, wy0, wz0},
                   {wx0, wy1, wz0}, {wx0, wy1, wz1},
                   {-1, 0, 0});
        // +X
        if (!solidAt(lx + 1, ly, lz))
          emitQuad(verts, indices,
                   {wx1, wy0, wz0}, {wx1, wy0, wz1},
                   {wx1, wy1, wz1}, {wx1, wy1, wz0},
                   {1, 0, 0});
        // -Z
        if (!solidAt(lx, ly, lz - 1))
          emitQuad(verts, indices,
                   {wx1, wy0, wz0}, {wx0, wy0, wz0},
                   {wx0, wy1, wz0}, {wx1, wy1, wz0},
                   {0, 0, -1});
        // +Z
        if (!solidAt(lx, ly, lz + 1))
          emitQuad(verts, indices,
                   {wx0, wy0, wz1}, {wx1, wy0, wz1},
                   {wx1, wy1, wz1}, {wx0, wy1, wz1},
                   {0, 0, 1});
      }
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

  // Clouds drift along +X. Rather than baking the offset into vertex positions
  // (which would force a full remesh every frame), the mesh lives in a static
  // grid space and the drift rides on the model matrix.
  const float drift = cloudTime * g_settings.cloud_speed * kCloudDrift;

  // Camera's cell in grid space — the mesh only needs rebuilding when this
  // changes, or when the band's shape settings are edited live.
  int camCX = static_cast<int>(std::floor((cameraPos.x + drift) / kCloudCell));
  int camCZ = static_cast<int>(std::floor(cameraPos.z / kCloudCell));

  if (camCX != cloud_last_cx || camCZ != cloud_last_cz ||
      g_settings.cloud_height    != cloud_last_height ||
      g_settings.cloud_thickness != cloud_last_thickness ||
      g_settings.fog_end         != cloud_last_fog_end) {
    rebuildCloudMesh(camCX, camCZ);
    cloud_last_cx        = camCX;
    cloud_last_cz        = camCZ;
    cloud_last_height    = g_settings.cloud_height;
    cloud_last_thickness = g_settings.cloud_thickness;
    cloud_last_fog_end   = g_settings.fog_end;
  }
  if (cloud_index_count == 0) return;

  glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(-drift, 0, 0));

  // Disable face culling so all sides are visible regardless of view angle
  glDisable(GL_CULL_FACE);

  cloud_shader->use();
  cloud_shader->setMat4("uModel", model);
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
    const std::vector<Chunk *> &chunks,
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
  stats.chunks_drawn = static_cast<int>(chunks.size());
  block_shader->setFloat("uAlpha", 1.0f);
  for (Chunk *chunk : chunks) {
    if (!chunk) continue;
    chunk->getMesh().renderOpaque();
  }

  // ── Pass 2: Transparent geometry (blended) ────────────────────────────────
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  block_shader->setFloat("uAlpha", 0.75f);
  for (Chunk *chunk : chunks) {
    if (!chunk) continue;
    chunk->getMesh().renderTransparent();
  }

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}
