#pragma once

#include "biome/biome.hpp"

class TropicalBiome: public Biome {
public:
  TropicalBiome();
  BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock(int y) override;
  void spawnDecorations(Chunk &chunk) override;
};

