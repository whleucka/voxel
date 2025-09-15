#pragma once

#include "chunk.hpp"
#include "render_ctx.hpp"
#include "texture.hpp"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ChunkKey {
  int x, z;
  bool operator==(const ChunkKey &other) const {
    return x == other.x && z == other.z;
  }
};

struct ChunkKeyHash {
  std::size_t operator()(const ChunkKey &k) const {
    // hash x,z
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
  World(Texture &block_atlas);
  ~World();

  const int chunk_width = 16;
  const int chunk_length = 16;
  const int chunk_height = 256;
  const int render_distance = 15;
  const int max_chunks_per_frame = 6; // how many chunks to load per frame
  const int sea_level = 42; // sea below y
  const int snow_height = 70; // snow above y
  const size_t max_cache = 256;

  void update(glm::vec3 camera_pos);
  void draw(renderCtx &ctx);
  BlockType getBlock(int x, int y, int z);
  Chunk *getChunk(int chunk_x, int chunk_y);
  int getChunkCount() const;
  float getMaxChunks() const;
  bool raycast(const glm::vec3 &start, const glm::vec3 &dir, float max_dist,
               glm::ivec3 &block_pos, glm::ivec3 &face_normal);
  void removeBlock(int x, int y, int z);
  void addBlock(int x, int y, int z, BlockType type);

private:
  std::unordered_map<ChunkKey, Chunk *, ChunkKeyHash>
      chunks; // currently rendered
  std::unordered_map<ChunkKey, Chunk *, ChunkKeyHash>
      cache; // inactive but saved
  std::unordered_set<ChunkKey, ChunkKeyHash> _loading_q; // being generated
  Texture &block_atlas;
  // Thread stuff
  std::vector<std::thread> _threads;
  std::queue<ChunkKey> _load_q;
  std::mutex _load_q_mutex;
  std::condition_variable _load_q_cv;
  std::queue<Chunk *> _generated_q;
  std::mutex _generated_q_mutex;
  bool _should_stop = false;

  void loadChunk(int chunk_x, int chunk_z);
  void unloadChunk(const ChunkKey &key);
  void startThreads();
  void stopThreads();
  void threadLoop();
};
