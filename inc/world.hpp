#pragma once

#include "chunk.hpp"
#include "render_ctx.hpp"
#include "texture.hpp"
#include <unordered_map>

struct ChunkKey {
    int x, z;
    bool operator==(const ChunkKey &other) const {
        return x == other.x && z == other.z;
    }
};

struct ChunkKeyHash {
    std::size_t operator()(const ChunkKey &k) const {
        return std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) << 1);
    }
};

/*
 * world.hpp
 *
 * 3d minecraft-like voxel game world
 * 
 */
class World {
public:
  World(Texture &atlas);
  ~World();
  void update(float dt, glm::vec3 camera_pos);
  void draw(renderCtx &ctx);
  BlockType getBlock(int x, int y, int z);
  int getChunkCount() const;
  void loadChunk(int chunk_x, int chunk_z);
  void unloadChunk(const ChunkKey &key);
  Texture& atlas;

private:
  // Using unordered_map for no out-of-range crashes
  std::unordered_map<ChunkKey, Chunk*, ChunkKeyHash> chunks;
  int width = 10;
  int length = 10;
};
