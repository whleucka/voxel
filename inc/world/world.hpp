#pragma once

#include "chunk/chunk.hpp"
#include "render/renderer.hpp"
#include "world/game_clock.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include "util/thread_pool.hpp"

class World {
public:
  World();
  ~World();

  void init();
  void update(float delta_time);
  void render(glm::mat4 &view, glm::mat4 &projection);
  void addChunk(int x, int z);
  std::shared_ptr<Chunk> getChunk(int x, int z);
  std::shared_ptr<const Chunk> getChunk(int x, int z) const;

private: 
  ThreadPool thread_pool;
  std::mutex chunks_mutex;
  GameClock game_clock;
  Renderer *renderer = nullptr;
  std::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash> chunks;
  std::queue<std::shared_ptr<Chunk>> generation_queue; // Queue for mesh generation
  std::queue<std::shared_ptr<Chunk>> upload_queue; // Queue for GPU upload
};
