#pragma once

#include "biome.hpp"

class OceanBiome : public Biome {
public:
  void generateTerrain(Chunk &chunk) override;
  void spawnDecorations(Chunk &chunk) override;
};
