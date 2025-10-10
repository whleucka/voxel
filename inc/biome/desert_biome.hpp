#pragma once

#include "biome/biome.hpp"

class DesertBiome: public Biome {
public:
  DesertBiome();
  ~DesertBiome() override = default;
  // BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock(int y) override;
  void spawnDecorations(Chunk &chunk) override;
};
