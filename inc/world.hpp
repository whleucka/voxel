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
#include "greedy_mesher.hpp"

struct PendingUpload {
  uint64_t chunk_key;
  Mesh* targetMesh;
  CpuMesh mesh;
};

struct GeneratedData {
    uint64_t key;
    std::unique_ptr<Chunk> chunk;
    CpuMesh opaque_mesh;
    CpuMesh transparent_mesh;
};

class World {
public:
  World();
  ~World();

  std::vector<Chunk *> getVisibleChunks(const Camera &camera);
  void update(const glm::vec3 &camera_pos);
  void processUploads();
  BlockType getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, BlockType type);

private:
  // Main-thread only
  robin_hood::unordered_map<uint64_t, std::unique_ptr<Chunk>> chunks;

  // Chunks currently being generated in the pool (guarded by main-thread only)
  robin_hood::unordered_set<uint64_t> loading;

  // Results from worker threads waiting to be promoted to `chunks`
  std::deque<GeneratedData> pending_generated;
  std::mutex pending_mutex; // guards pending_generated
  mutable std::mutex chunks_mutex; // guards chunks

  std::deque<PendingUpload> pending_uploads;

  ThreadPool thread_pool;

  int render_distance = 5;

  void loadChunk(int cx, int cz);
  void unloadChunk(int cx, int cz);

  // Promote some completed worker outputs into the world (main thread)
  void promotePendingGenerated(int budget);


  static uint64_t makeChunkKey(int cx, int cz) {
    return (uint64_t(uint32_t(cx)) << 32) | uint32_t(cz);
  }
};
