#pragma once

#include "chunk/chunk.hpp"
#include "render/renderer.hpp"
#include "world/game_clock.hpp"
#include "world/terrain_manager.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>

class World {
public:
  World();
  ~World();

  void init();
  void update(float delta_time);
  void render(glm::mat4 &view, glm::mat4 &projection);
  void addChunk(int x, int z);
  Chunk* getChunk(int x, int z);
  const Chunk* getChunk(int x, int z) const;

private: 
  TerrainManager terrain_manager;
  GameClock game_clock;
  Renderer *renderer = nullptr;
  std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash> chunks;
};
