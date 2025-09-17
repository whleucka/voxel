#include "renderer.hpp"
#include "stb_image.h"
#include "texture_manager.hpp"

Renderer::Renderer() : block_shader(nullptr) {}

Renderer::~Renderer() { delete block_shader; }

void Renderer::init() {
  // Load the block shader
  block_shader = new Shader("shaders/block.vert", "shaders/block.frag");
}

void Renderer::draw(const std::vector<Chunk *> &chunks, const Camera &camera,
                    int screen_width, int screen_height) {
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
      1000.0f);
  glm::mat4 view = camera.getViewMatrix();
  block_shader->setMat4("projection", projection);
  block_shader->setMat4("view", view);

  // Lighting / fog
  block_shader->setVec3("lightDir", glm::vec3(0.5f, -1.0f, 0.5f));
  block_shader->setVec3("ambientColor", glm::vec3(0.2f));
  block_shader->setFloat("sunStrength", 1.0f);
  block_shader->setVec3("cameraPos", camera.getPosition());
  block_shader->setVec3("fogColor", glm::vec3(0.5f, 0.6f, 0.7f));
  block_shader->setFloat("fogStart", 100.0f);
  block_shader->setFloat("fogEnd", 1000.0f);

  // —— PASS 1: Opaque ——
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  for (const Chunk *c : chunks) {
    // IMPORTANT: set model (translate chunk into world)
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
}
