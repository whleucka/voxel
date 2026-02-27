#pragma once

#include "block/block_type.hpp"
#include "chunk/chunk.hpp"
#include "player/player.hpp"
#include "render/renderer.hpp"
#include "robin_hood/robin_hood.h"
#include "util/lock.hpp"
#include "util/thread_pool.hpp"
#include "world/game_clock.hpp"
#include <glm/glm.hpp>
#include <memory>

// Result of a DDA raycast into the voxel world
struct RaycastResult {
  bool hit = false;
  glm::ivec3 block_pos{0};   // position of the block that was hit
  glm::ivec3 normal{0};      // face normal (for placement: place at block_pos + normal)
  float distance = 0.0f;     // distance from ray origin to hit
  BlockType block_type = BlockType::AIR;
};

class World {
public:
  World();
  ~World() = default;

  void init();
  void update(float delta_time);
  void render(glm::mat4 &view, glm::mat4 &projection, float timeOfDay);
  void addChunk(int x, int z);
  std::shared_ptr<Chunk> getChunk(int x, int z);
  std::shared_ptr<const Chunk> getChunk(int x, int z) const;
  std::unique_ptr<Player> &getPlayer() { return player; }
  size_t getChunkCount() const;
  BlockType getBlockAt(const glm::vec3& worldPos) const;
  bool isSolidBlock(int bx, int by, int bz) const;
  void setBlockAt(const glm::ivec3& worldPos, BlockType type);

  // DDA raycast: find the first solid block along a ray
  RaycastResult raycast(const glm::vec3& origin, const glm::vec3& direction,
                        float max_distance) const;

private:
  void rebuildChunk(int cx, int cz);
  void preloadChunks();
  bool isChunkInFrustum(const glm::vec4 planes[6], const glm::vec3 &min, const glm::vec3 &max);
  std::vector<glm::ivec2> generateSpiralOrder(int radius);
  std::vector<glm::ivec2> spiral_offsets;
  void updateLoadedChunks();
  void unloadChunk(int x, int z);

  ThreadPool gen_pool{6};
  ThreadPool mesh_pool{6};
  mutable SharedMutex chunks_mutex;

  GameClock game_clock;
  std::unique_ptr<Renderer> renderer;
  std::unique_ptr<Player> player;

  robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash>
      chunks;
  std::queue<std::shared_ptr<Chunk>> mesh_queue;
  std::queue<std::shared_ptr<Chunk>> upload_queue;

  int last_chunk_x = 0;
  int last_chunk_z = 0;
};
