#include "renderer.hpp"
#include "stb_image.h"
#include "texture_manager.hpp"
#include "world.hpp"

const glm::vec3 night(0.0f, 11.0f / 255.0f, 28.0f / 255.0f);
const glm::vec3 day(0.4f, 0.7f, 1.0f);
const glm::vec3 sunset(1.0f, 0.5f, 0.2f);

Renderer::Renderer() : block_shader(nullptr) {}

Renderer::~Renderer() { delete block_shader; }

void Renderer::init() {
  // Load the block shader
  block_shader = new Shader("shaders/block.vert", "shaders/block.frag");
}

void Renderer::draw(const std::vector<Chunk *> &chunks, const Camera &camera,
                    int screen_width, int screen_height, float time_fraction) {
  
  glm::vec3 sky_color;
  if (time_fraction < 0.25f) { // Midnight to sunrise
    sky_color = glm::mix(night, sunset, time_fraction / 0.25f);
  }
  else if (time_fraction < 0.5f) { // Sunrise to noon
    sky_color = glm::mix(sunset, day, (time_fraction - 0.25f) / 0.25f);
  }
  else if (time_fraction < 0.75f) { // Noon to sunset
    sky_color = glm::mix(day, sunset, (time_fraction - 0.5f) / 0.25f);
  }
  else { // Sunset to midnight
    sky_color = glm::mix(sunset, night, (time_fraction - 0.75f) / 0.25f);
  }

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
      glm::radians(45.0f), (float)screen_width / (float)screen_height, 0.5f,
      512.0f);
  glm::mat4 view = camera.getViewMatrix();
  block_shader->setMat4("projection", projection);
  block_shader->setMat4("view", view);

  // Lighting / fog
  block_shader->setVec3("lightDir", glm::vec3(0.5f, -1.0f, 0.5f));
  block_shader->setVec3("ambientColor", glm::vec3(0.2f));
  block_shader->setFloat("sunStrength", 1.0f);
  block_shader->setVec3("cameraPos", camera.getPosition());
  block_shader->setVec3("fogColor", glm::vec3(0.5f, 0.6f, 0.7f));
  // How far you can see in world units (radius to the outer ring of chunks)
  float chunkSize = float(Chunk::W); // or 0.5f*(Chunk::W + Chunk::L)
  float radiusWU =
      (World::render_distance + 0.5f) * chunkSize; // +0.5 for a little buffer

  // Place fog near the edge of visibility
  float fogStart = 0.70f * radiusWU; // starts ~70% of the way out
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
}
