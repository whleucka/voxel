#pragma once

#include "biome.hpp"
#include "tree_spawner.hpp"

class TropicalBiome : public Biome {
public:
  TropicalBiome();
  void generateTerrain(Chunk &chunk) override;
  void spawnDecorations(Chunk &chunk) override;

private:
  TreeSpawner m_palm_tree_spawner;
  TreeSpawner m_oak_tree_spawner;
};