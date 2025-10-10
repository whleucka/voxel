#pragma once

#include "biome/biome.hpp"

class OceanBiome: public Biome {
public:
  OceanBiome();
  ~OceanBiome() override = default;
  // BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock(int y) override;
  void spawnDecorations(Chunk &chunk) override;
};
