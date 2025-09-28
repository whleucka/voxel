#pragma once

#include "biome.hpp"

class DesertBiome : public Biome {
public:
  void generateTerrain(Chunk &chunk) override;
  void spawnDecorations(Chunk &chunk) override;
};
