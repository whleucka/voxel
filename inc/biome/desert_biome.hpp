#pragma once

#include "biome/biome.hpp"

class DesertBiome: public Biome {
public:
  DesertBiome();
  BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock() override;
  void spawnDecorations(Chunk &chunk) override;
};
