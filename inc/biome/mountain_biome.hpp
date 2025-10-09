#pragma once

#include "biome/biome.hpp"

class MountainBiome: public Biome {
public:
  MountainBiome();
  BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock(int y) override;
  void spawnDecorations(Chunk &chunk) override;
};
