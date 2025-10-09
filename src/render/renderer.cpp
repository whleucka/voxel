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

  for (auto &[key, chunk] : chunks) {
    block_shader->setMat4("model", chunk->getModelMatrix());
    block_shader->setMat4("view", view);
    block_shader->setMat4("projection", projection);

    chunk->getMesh().render();
  }
}
