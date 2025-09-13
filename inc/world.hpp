#pragma once

#include "chunk.hpp"
#include "render_ctx.hpp"
#include "texture.hpp"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

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
  World(Texture &block_atlas);
  ~World();
  void update(glm::vec3 camera_pos);
  void draw(renderCtx &ctx);
  BlockType getBlock(int x, int y, int z);
  Chunk *getChunk(int chunk_x, int chunk_y);
  int getChunkCount() const;

private:
  void start_threads();
  void stop_threads();
  void thread_loop();

  std::unordered_map<ChunkKey, Chunk *, ChunkKeyHash>
      chunks; // currently rendered
  std::unordered_map<ChunkKey, Chunk *, ChunkKeyHash>
      cache; // inactive but saved
  Texture &block_atlas;
  void loadChunk(int chunk_x, int chunk_z);
  void unloadChunk(const ChunkKey &key);

  // --- thread management ---
  std::vector<std::thread> _threads;
  std::queue<ChunkKey> _load_q;
  std::mutex _load_q_mutex;
  std::condition_variable _load_q_cv;

  std::queue<Chunk *> _generated_q;
  std::mutex _generated_q_mutex;

  bool _should_stop = false;
};
