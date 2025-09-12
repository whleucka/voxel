#include "world.hpp"
#include "coord.hpp"
#include "render_ctx.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const int chunk_width = 16;
const int chunk_length = 16;
const int chunk_height = 256;
const int render_distance = 1;

inline int worldToChunkCoord(int pos, int chunkSize) {
  return (pos >= 0) ? (pos / chunkSize) : ((pos + 1) / chunkSize - 1);
}

World::World(Texture &a) : block_atlas(a) {}

World::~World() {}

void World::loadChunk(int x, int z) {
  ChunkKey key{x, z};
  std::cout << "LOAD CHUNK (" << key.x << ", " << key.z << ")" << std::endl;

  // Already active
  if (chunks.find(key) != chunks.end())
    return;

  // Reactivate from cache
  auto it = cache.find(key);
  if (it != cache.end()) {
    chunks[key] = it->second;
    cache.erase(it);
    return;
  }

  // Fresh generate
  Chunk *chunk = new Chunk(chunk_width, chunk_length, chunk_height, x, z, this);
  chunk->generateMesh(block_atlas);
  chunks[key] = chunk;
}

void World::unloadChunk(const ChunkKey &key) {
  std::cout << "LOAD CHUNK (" << key.x << ", " << key.z << ")" << std::endl;
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    cache[key] = it->second; // move into cache
    chunks.erase(it);        // remove from active
  }
}

int World::getChunkCount() const { return chunks.size(); }

Chunk *World::getChunk(int chunk_x, int chunk_z) {
  ChunkKey key{chunk_x, chunk_z};
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    return it->second; // loaded
  }
  return nullptr; // not loaded
}

void World::update(float dt, glm::vec3 camera_pos) {
  int cam_cx = worldToChunkCoord(static_cast<int>(camera_pos.x), chunk_width);
  int cam_cz = worldToChunkCoord(static_cast<int>(camera_pos.z), chunk_length);

  // Load any missing activeChunks in render distance
  int chunksBuiltThisFrame = 0;
  const int maxChunksPerFrame = 1;

  for (int dx = -render_distance; dx <= render_distance; dx++) {
    for (int dz = -render_distance; dz <= render_distance; dz++) {
      int cx = cam_cx + dx;
      int cz = cam_cz + dz;

      ChunkKey key{cx, cz};
      if (chunks.find(key) == chunks.end()) {
        loadChunk(cx, cz);
        chunksBuiltThisFrame++;

        if (chunksBuiltThisFrame >= maxChunksPerFrame) {
          return; // bail early, do the rest next frame
        }
      }
    }
  }

  // Unload chunks that are too far away
  std::vector<ChunkKey> toUnload;
  for (auto &[key, chunk] : chunks) {
    int distX = key.x - cam_cx;
    int distZ = key.z - cam_cz;
    if (std::abs(distX) > render_distance ||
        std::abs(distZ) > render_distance) {
      toUnload.push_back(key);
    }
  }

  for (auto &key : toUnload) {
    unloadChunk(key);
  }
}

void World::draw(renderCtx &ctx) {
  if (chunks.empty())
    return;

  glUseProgram(ctx.block_shader.ID);

  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "view"), 1,
                     GL_FALSE, glm::value_ptr(ctx.view));
  glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(ctx.proj));
  for (auto &[key, chunk] : chunks) {
    glm::mat4 model =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(key.x * chunk_width, 0, key.z * chunk_length));
    glUniformMatrix4fv(glGetUniformLocation(ctx.block_shader.ID, "model"), 1,
                       GL_FALSE, glm::value_ptr(model));
    chunk->draw(ctx.block_shader);
  }
}

BlockType World::getBlock(int x, int y, int z) {
  const int cx = worldToChunk(x, chunk_width);
  const int cz = worldToChunk(z, chunk_length);

  auto it = chunks.find(ChunkKey{cx, cz});
  if (it == chunks.end()) {
    // TEMP debug to prove whether the miss is here
    // std::cout << "MISS: world (" << x << "," << y << "," << z
    //           << ") -> chunk (" << cx << "," << cz << ")\n";
    return BlockType::AIR;
  }

  const int lx = worldToLocal(x, chunk_width);
  const int lz = worldToLocal(z, chunk_length);

  return it->second->getBlock(lx, y, lz);
}
