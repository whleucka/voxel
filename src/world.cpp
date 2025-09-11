#include "world.hpp"
#include "render_ctx.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

World::World(const Texture &atlas) {
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      chunks.push_back(new Chunk(16, 16, 50, atlas, i, j));
    }
  }
}

World::~World() {
  for (auto chunk : chunks) {
    delete chunk;
  }
  chunks.clear();
}

void World::update(float dt) {}

void World::draw(renderCtx &ctx) {
  glUseProgram(ctx.blockShader.ID);

  glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "view"), 1,
                     GL_FALSE, glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(ctx.proj));

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, glm::vec3(i * 16, 0, j * 16));
      glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "model"), 1,
                         GL_FALSE, glm::value_ptr(model));
      chunks[i * 5 + j]->draw(ctx.blockShader);
    }
  }
}
