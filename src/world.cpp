#include "world.hpp"
#include "render_ctx.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

World::World(const Texture &atlas) { m_chunk = new Chunk(atlas); }

World::~World() {
  delete m_chunk;
  m_chunk = nullptr;
}

void World::update(float dt) {}

void World::draw(renderCtx &ctx) {
  glUseProgram(ctx.blockShader.ID);

  glm::mat4 model(1.0f); // identity
  glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "model"), 1,
                     GL_FALSE, glm::value_ptr(model));
  glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "view"), 1, GL_FALSE,
                     glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(ctx.proj));

  m_chunk->draw(ctx.blockShader);
}