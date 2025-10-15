#pragma once

#include "chunk/chunk.hpp"
#include "render/renderer.hpp"
#include "world/game_clock.hpp"
#include "player/player.hpp"
#include "util/lock.hpp"
#include <glm/glm.hpp>
#include <memory>
#include "robin_hood/robin_hood.h"
#include "util/thread_pool.hpp"

class World {
public:
  World();
  ~World() = default;

  void init();
  void update(float delta_time);
  void render(glm::mat4 &view, glm::mat4 &projection);
  void addChunk(int x, int z);
  std::shared_ptr<Chunk> getChunk(int x, int z);
  std::shared_ptr<const Chunk> getChunk(int x, int z) const;
  std::unique_ptr<Player> &getPlayer() { return player; }
  size_t getChunkCount() const;

private:
  void updateLoadedChunks();
  void unloadChunk(int x, int z);

  ThreadPool gen_pool{4};
  ThreadPool mesh_pool{4};
  mutable SharedMutex chunks_mutex;
  GameClock game_clock;
  std::unique_ptr<Renderer> renderer;
  std::unique_ptr<Player> player;

  robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> chunks;
  std::queue<std::shared_ptr<Chunk>> mesh_queue;
  std::queue<std::shared_ptr<Chunk>> upload_queue;

  int last_chunk_x = 0;
  int last_chunk_z = 0;
};
