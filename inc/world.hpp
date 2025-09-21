#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>
#include <optional>

#include <glm/glm.hpp>

#include "camera.hpp"
#include "chunk.hpp"
#include "greedy_mesher.hpp"
#include "robin_hood.h"
#include "thread_pool.hpp"

struct PendingUpload {
  uint64_t chunk_key;
  Mesh *targetMesh;
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

  static constexpr int render_distance = 15;

  std::vector<Chunk *> getVisibleChunks(const Camera &cam, int width, int height);
  void update(const Camera &cam);
  void processUploads();
  void generateChunkData(int cx, int cz);
  void generateChunkMesh(int cx, int cz);
  void processAllUploads();
  int getHighestBlock(int x, int z);
  BlockType getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, BlockType type);

  std::optional<std::tuple<glm::ivec3, glm::ivec3>> raycast(const glm::vec3 &start, const glm::vec3 &direction, float max_dist) const;

  // How many chunks have loaded
  size_t getLoadedChunkCount() const { return chunks.size(); }

  // uint64_t chunk key -- made of 2 32 bit integers
  static uint64_t makeChunkKey(int cx, int cz) {
    return (uint64_t(uint32_t(cx)) << 32) | uint32_t(cz);
  }
private:
  // Load chunks algo
  void spiralLoadOrderCircle(int r, std::vector<std::pair<int, int>> &out);

  // Main-thread only
  robin_hood::unordered_map<uint64_t, std::unique_ptr<Chunk>> chunks;

  // Chunks currently being generated in the pool (guarded by main-thread only)
  robin_hood::unordered_set<uint64_t> loading;

  // Results from worker threads waiting to be promoted to `chunks`
  std::deque<GeneratedData> pending_generated;
  std::mutex pending_mutex;        // guards pending_generated
  mutable std::mutex chunks_mutex; // guards chunks

  std::deque<PendingUpload> pending_uploads;

  ThreadPool thread_pool;

  void loadChunk(int cx, int cz);
  void unloadChunk(int cx, int cz);

  // Promote some completed worker outputs into the world (main thread)
  void promotePendingGenerated(int budget);
};
