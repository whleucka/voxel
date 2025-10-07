#pragma once

#include "biome/biome.hpp"

class PlainsBiome: public Biome {
public:
  PlainsBiome();
  BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock() override;
  void spawnDecorations(Chunk &chunk) override;
};
