#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "chunk.hpp"
#include "robin_hood.h"
#include "thread_pool.hpp"
#include "camera.hpp"

class World {
public:
  World();
  ~World();

  std::vector<Chunk*> getVisibleChunks(const Camera& camera);
  void update(const glm::vec3 &camera_pos);
  void generateMeshes();
  BlockType getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, BlockType type);

private:
  robin_hood::unordered_map<uint64_t, std::unique_ptr<Chunk>> chunks;
  ThreadPool thread_pool;

  int render_distance = 8;

  void loadChunk(int cx, int cz);
  void unloadChunk(int cx, int cz);
  static uint64_t makeChunkKey(int cx, int cz) {
    return (uint64_t(uint32_t(cx)) << 32) | uint32_t(cz);
  }
};