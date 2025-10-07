#pragma once

#include <glm/glm.hpp>

class TerrainManager {
public:
  TerrainManager();
  ~TerrainManager();
  int getHeight(glm::vec2 pos, float *biome_noise_out = nullptr);
};
