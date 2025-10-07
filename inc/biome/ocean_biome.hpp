#pragma once

#include "biome/biome.hpp"

class OceanBiome: public Biome {
public:
  OceanBiome();
  BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock() override;
  void spawnDecorations(Chunk &chunk) override;
};
