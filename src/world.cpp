#include "world.hpp"
#include "render_ctx.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const int chunk_width = 16;
const int chunk_length = 16;
const int chunk_height = 256;
const int render_distance = 5;

inline int worldToChunkCoord(int pos, int chunkSize) {
    // works for negatives too
    return (pos >= 0) ? (pos / chunkSize) : ((pos + 1) / chunkSize - 1);
}

World::World(Texture &a) : atlas(a) {
}

World::~World() {
}

void World::loadChunk(int chunk_x, int chunk_z) {
  ChunkKey key{chunk_x, chunk_z};
  if (chunks.find(key) == chunks.end()) {
    Chunk* chunk = new Chunk(chunk_width, chunk_length, chunk_height, chunk_x, chunk_z, this);
    chunk->generateMesh(atlas);
    chunks[key] = chunk;
  }
}

void World::unloadChunk(const ChunkKey &key) {
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    delete it->second;
    chunks.erase(it);
  }
}

int World::getChunkCount() const {
  return chunks.size();
}

void World::update(float dt, glm::vec3 camera_pos) {
  int cam_cx = worldToChunkCoord(static_cast<int>(camera_pos.x), chunk_width);
  int cam_cz = worldToChunkCoord(static_cast<int>(camera_pos.z), chunk_length);

  // Load any missing chunks in render distance
  for (int dx = -render_distance; dx <= render_distance; dx++) {
    for (int dz = -render_distance; dz <= render_distance; dz++) {
      int cx = cam_cx + dx;
      int cz = cam_cz + dz;

      ChunkKey key{cx, cz};
      if (chunks.find(key) == chunks.end()) {
        loadChunk(cx, cz);
      }
    }
  }

  // Unload chunks that are too far away
  std::vector<ChunkKey> toUnload;
  for (auto &[key, chunk] : chunks) {
    int distX = key.x - cam_cx;
    int distZ = key.z - cam_cz;
    if (std::abs(distX) > render_distance || std::abs(distZ) > render_distance) {
      toUnload.push_back(key);
    }
  }

  for (auto &key : toUnload) {
    unloadChunk(key);
  }
}

void World::draw(renderCtx &ctx) {
  if (chunks.empty()) return;

  glUseProgram(ctx.block_shader.ID);

  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "view"), 1,
      GL_FALSE, glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "projection"), 1,
      GL_FALSE, glm::value_ptr(ctx.proj));

  for (auto &[key, chunk] : chunks) {
    glm::mat4 model = glm::mat4(1.0f);
    // place chunk in world based on its (x,z) indices
    model = glm::translate(model, glm::vec3(key.x * chunk_width, 0, key.z * chunk_length));

    glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "model"), 1,
        GL_FALSE, glm::value_ptr(model));
    chunk->draw(ctx.block_shader);
  }
}

BlockType World::getBlock(int x, int y, int z) {
    int chunk_x = x / chunk_width;
    int chunk_z = z / chunk_length;

    ChunkKey key{chunk_x, chunk_z};
    auto it = chunks.find(key);
    if (it == chunks.end()) return BlockType::AIR;

    return it->second->getBlock(x % chunk_width, y, z % chunk_length);
}
