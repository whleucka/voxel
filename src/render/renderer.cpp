#include "render/renderer.hpp"
#include "chunk/chunk.hpp"
#include "render/texture_manager.hpp"

Renderer::Renderer() : texture_manager(new TextureManager) {}

Renderer::~Renderer() {
  delete block_shader;
  delete texture_manager;
}

void Renderer::init() {
  block_shader =
      new Shader("assets/shaders/block.vert", "assets/shaders/block.frag");
  texture_manager->loadAtlas("res/block_atlas.png");
}

void Renderer::drawChunks(const robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> &chunks,
                          const glm::mat4 &view, const glm::mat4 &projection) {
  block_shader->use();
  texture_manager->bind(GL_TEXTURE0);
  block_shader->setInt("uTexture", 0);
  block_shader->setMat4("view", view);
  block_shader->setMat4("projection", projection);

  // Pass 1: Draw all opaque geometry
  block_shader->setFloat("uAlpha", 1.0f);
  for (auto &[key, chunk] : chunks) {
    if (!chunk) continue;
    chunk->getMesh().renderOpaque();
  }

  // Pass 2: Draw all transparent geometry with blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE); // Don't write to depth buffer for transparent

  block_shader->setFloat("uAlpha", 0.6f);
  for (auto &[key, chunk] : chunks) {
    if (!chunk) continue;
    chunk->getMesh().renderTransparent();
  }

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}
