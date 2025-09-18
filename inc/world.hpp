#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "camera.hpp"
#include "chunk.hpp"
#include "robin_hood.h"
#include "thread_pool.hpp"

class World {
public:
  World();
  ~World();

  std::vector<Chunk *> getVisibleChunks(const Camera &camera);
  void update(const glm::vec3 &camera_pos);
  void generateMeshes();
  BlockType getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, BlockType type);

private:
  // Main-thread only
  robin_hood::unordered_map<uint64_t, std::unique_ptr<Chunk>> chunks;

  // Chunks that need meshing (main-thread)
  robin_hood::unordered_set<uint64_t> dirty;

  // Chunks currently being generated in the pool (guarded by main-thread only)
  robin_hood::unordered_set<uint64_t> loading;

  // Results from worker threads waiting to be promoted to `chunks`
  std::deque<std::pair<uint64_t, std::unique_ptr<Chunk>>> pending_generated;
  std::mutex pending_mutex; // guards pending_generated

  ThreadPool thread_pool;

  int render_distance = 10;

  void loadChunk(int cx, int cz);
  void unloadChunk(int cx, int cz);

  // Promote some completed worker outputs into the world (main thread)
  void promotePendingGenerated(int budget);
  void queueMeshUpload(uint64_t key, bool transparent);


  static uint64_t makeChunkKey(int cx, int cz) {
    return (uint64_t(uint32_t(cx)) << 32) | uint32_t(cz);
  }
};
