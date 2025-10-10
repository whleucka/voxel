#pragma once

#include "biome/biome.hpp"

class PlainsBiome: public Biome {
public:
  PlainsBiome();
  ~PlainsBiome() override = default;
  // BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock(int y) override;
  void spawnDecorations(Chunk &chunk) override;
};
