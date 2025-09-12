#include "world.hpp"
#include "render_ctx.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const int chunk_width = 16;
const int chunk_length = 16;
const int chunk_height = 256;

World::World(const Texture &atlas) {
  for (int i = 0; i < width; i++) {
    for (int j = 0; j < length; j++) {
      chunks.push_back(new Chunk(chunk_width, chunk_length, chunk_height, i, j, this));
    }
  }

  for (auto chunk : chunks) {
    chunk->generateMesh(atlas);
  }
}

World::~World() {
  for (auto chunk : chunks) {
    delete chunk;
  }
  chunks.clear();
}

int World::getChunkCount() const {
  return chunks.size();
}

void World::update(float) {}

void World::draw(renderCtx &ctx) {
  glUseProgram(ctx.block_shader.ID);

  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "view"), 1,
                     GL_FALSE, glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(ctx.proj));

  for (int i = 0; i < width; i++) {
    for (int j = 0; j < length; j++) {
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, glm::vec3(i * chunk_width, 0, j * chunk_length));
      glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "model"), 1,
                         GL_FALSE, glm::value_ptr(model));
      chunks[i * length + j]->draw(ctx.block_shader);
    }
  }
}

BlockType World::getBlock(int x, int y, int z) {
  int chunk_x = x / chunk_width;
  int chunk_z = z / chunk_width;
  int block_x = x % chunk_width;
  int block_z = z % chunk_width;

  if (x < 0) {
    chunk_x = (x + 1) / chunk_width - 1;
    block_x = x - chunk_x * chunk_width;
  } else {
    chunk_x = x / chunk_width;
    block_x = x % chunk_width;
  }

  if (z < 0) {
    chunk_z = (z + 1) / chunk_width - 1;
    block_z = z - chunk_z * chunk_width;
  } else {
    chunk_z = z / chunk_width;
    block_z = z % chunk_width;
  }

  if (chunk_x < 0 || chunk_x >= width || chunk_z < 0 || chunk_z >= length) {
    return BlockType::AIR;
  }

  return chunks[chunk_x * length + chunk_z]->getBlock(block_x, y, block_z);
}
