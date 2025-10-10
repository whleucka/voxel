#pragma once

#include "biome/biome.hpp"
#include "biome/tree_spawner.hpp"

class PlainsBiome : public Biome {
public:
  PlainsBiome();
  ~PlainsBiome() override = default;
  // BlockType generateInternalBlock(int x, int y, int z) override;
  BlockType generateTopBlock(int y) override;
  void spawnDecorations(Chunk &chunk) override;
private:
  TreeSpawner oak_tree_spawner;
};
