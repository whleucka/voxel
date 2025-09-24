#include "renderer.hpp"
#include "stb_image.h"
#include "texture_manager.hpp"
#include "world.hpp"

const glm::vec3 night(0.0f / 255.0f, 0.0f / 255.0f, 3.0f / 255.0f);
const glm::vec3 day(0.4f, 0.7f, 1.0f);
const glm::vec3 sunset(1.0f, 0.5f, 0.2f);

Renderer::Renderer()
    : block_shader(nullptr), highlight_shader(nullptr), wire_cube(nullptr) {}

Renderer::~Renderer() {
  delete block_shader;
  delete highlight_shader;
  delete wire_cube;
}

void Renderer::init() {
  // Load the block shader
  block_shader = new Shader("shaders/block.vert", "shaders/block.frag");
  highlight_shader =
      new Shader("shaders/highlight.vert", "shaders/highlight.frag");
  wire_cube = new WireCube();
}

void Renderer::draw(const std::vector<Chunk *> &chunks, const Camera &camera,
                    int screen_width, int screen_height, float time_fraction) {

  float t = time_fraction; // 0..1
  float ang = t * 2.0f * glm::pi<float>() -
              glm::half_pi<float>(); // rises ~0.25, sets ~0.75
  glm::vec3 sunDir =
      glm::normalize(glm::vec3(std::cos(ang), std::sin(ang), 0.25f));

  float h = sunDir.y;

  // Two smoothsteps: twilight below/above horizon
  float duskDawnBelow =
      glm::smoothstep(-0.15f, 0.00f, h); // -0.15→0: night → sunset
  float dawnNoonAbove =
      glm::smoothstep(0.00f, 0.25f, h); // 0→0.25: sunset → day

  glm::vec3 sky_color;
  if (h <= 0.0f) {
    // Below horizon: mostly night, a touch of sunset near horizon
    sky_color = glm::mix(night, sunset, duskDawnBelow);
  } else {
    // Above horizon: start at sunset near horizon, fade to day as sun climbs
    sky_color = glm::mix(sunset, day, dawnNoonAbove);
  }

  float horizonGlow = 1.0f - glm::smoothstep(0.05f, 0.25f, std::abs(h));
  sky_color = glm::mix(sky_color, sunset, 0.25f * horizonGlow);

  glViewport(0, 0, screen_width, screen_height);
  glClearColor(sky_color.r, sky_color.g, sky_color.b, 1.0f);

  block_shader->use();

  // Bind atlas + sampler
  const Texture &atlas =
      TextureManager::getInstance().getTexture("block_atlas");
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, atlas.id);
  block_shader->setInt("texture_diffuse1", 0);

  // View/Projection
  glm::mat4 projection = glm::perspective(
      glm::radians(45.0f), (float)screen_width / (float)screen_height, 0.1f,
      512.0f);
  glm::mat4 view = camera.getViewMatrix();
  block_shader->setMat4("projection", projection);
  block_shader->setMat4("view", view);

  // Sun
  glm::vec3 sunColor = glm::mix(night, day, sunset);
  block_shader->setVec3("sunColor", sunColor);
  // 0 when sun is well below horizon, ~1 when up. (Softens twilight.)
  float sunVis = glm::clamp(glm::smoothstep(-0.02f, 0.10f, h), 0.0f, 1.0f);
  // Pass to shader
  block_shader->setFloat("uSunVis", sunVis);

  // Lighting
  block_shader->setVec3("lightDir", sunDir);
  block_shader->setVec3("ambientColor", glm::vec3(0.2f));
  block_shader->setFloat("sunStrength", 1.0f);
  block_shader->setVec3("cameraPos", camera.getPosition());

  // Fog
  block_shader->setVec3("fogColor",
                        glm::vec3(sky_color.r, sky_color.g, sky_color.b));
  // How far you can see in world units (radius to the outer ring of chunks)
  float chunkSize = float(Chunk::W); // or 0.5f*(Chunk::W + Chunk::L)
  float radiusWU =
      (World::render_distance + 1.0f) * chunkSize; // for a little buffer

  // Place fog near the edge of visibility
  float fogStart = 0.65f * radiusWU; // starts ~65% of the way out
  float fogEnd = 0.98f * radiusWU;   // fully fogged just before the edge

  block_shader->setFloat("fogStart", fogStart);
  block_shader->setFloat("fogEnd", fogEnd);

  // —— PASS 1: Opaque ——
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  for (const Chunk *c : chunks) {
    glm::mat4 model =
        glm::translate(glm::mat4(1.0f), glm::vec3(c->world_x * Chunk::W, 0.0f,
                                                  c->world_z * Chunk::L));
    block_shader->setMat4("model", model);
    c->opaqueMesh.draw(*block_shader);
  }

  // —— PASS 2: Transparent ——
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  for (const Chunk *c : chunks) {
    glm::mat4 model =
        glm::translate(glm::mat4(1.0f), glm::vec3(c->world_x * Chunk::W, 0.0f,
                                                  c->world_z * Chunk::L));
    block_shader->setMat4("model", model);
    c->transparentMesh.draw(*block_shader);
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND); // leave GL clean
  glEnable(GL_CULL_FACE); // Re-enable culling
}

void Renderer::drawHighlight(const Camera &camera, const glm::vec3 &block_pos) {
  highlight_shader->use();
  glm::mat4 projection =
      glm::perspective(glm::radians(45.0f), 1920.f / 1080.f, 0.1f, 512.0f);
  glm::mat4 view = camera.getViewMatrix();
  highlight_shader->setMat4("projection", projection);
  highlight_shader->setMat4("view", view);

  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, block_pos + glm::vec3(0.5f, 0.5f, 0.5f));
  model = glm::scale(model, glm::vec3(1.01f, 1.01f, 1.01f));
  highlight_shader->setMat4("model", model);

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glLineWidth(5.0f);
  wire_cube->draw();
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
