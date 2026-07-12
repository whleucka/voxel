#pragma once

#include "biome/biome.hpp"
#include "biome/tree_spawner.hpp"

class CherryBiome : public Biome {
public:
  CherryBiome();
  ~CherryBiome() override = default;
  BlockType generateTopBlock(int y) override;
  void spawnDecorations(Chunk &chunk) override;

private:
  TreeSpawner cherry_tree_spawner;
};
