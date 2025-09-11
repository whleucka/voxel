#include "world.hpp"
#include "render_ctx.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

World::World(const Texture &atlas) {
  for (int i = 0; i < width; i++) {
    for (int j = 0; j < length; j++) {
      chunks.push_back(new Chunk(16, 16, 64, i, j, this));
    }
  }

  for (auto chunk : chunks) {
    chunk->generate_mesh(atlas);
  }
}

World::~World() {
  for (auto chunk : chunks) {
    delete chunk;
  }
  chunks.clear();
}

void World::update(float) {}

void World::draw(renderCtx &ctx) {
  glUseProgram(ctx.blockShader.ID);

  glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "view"), 1,
                     GL_FALSE, glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(ctx.proj));

  for (int i = 0; i < width; i++) {
    for (int j = 0; j < length; j++) {
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, glm::vec3(i * 16, 0, j * 16));
      glUniformMatrix4fv(glGetUniformLocation(ctx.blockShader.ID, "model"), 1,
                         GL_FALSE, glm::value_ptr(model));
      chunks[i * length + j]->draw(ctx.blockShader);
    }
  }
}

BlockType World::get_block(int x, int y, int z) {
  int chunk_x = x / 16;
  int chunk_z = z / 16;
  int block_x = x % 16;
  int block_z = z % 16;

  if (x < 0) {
    chunk_x = (x + 1) / 16 - 1;
    block_x = x - chunk_x * 16;
  } else {
    chunk_x = x / 16;
    block_x = x % 16;
  }

  if (z < 0) {
    chunk_z = (z + 1) / 16 - 1;
    block_z = z - chunk_z * 16;
  } else {
    chunk_z = z / 16;
    block_z = z % 16;
  }

  if (chunk_x < 0 || chunk_x >= width || chunk_z < 0 || chunk_z >= length) {
    return BlockType::AIR;
  }

  return chunks[chunk_x * length + chunk_z]->get_block(block_x, y, block_z);
}
