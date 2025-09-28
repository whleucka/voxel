#pragma once

#include "biome.hpp"
#include "tree_spawner.hpp"

class ForestBiome : public Biome {
public:
  ForestBiome();
  void generateTerrain(Chunk &chunk) override;
  void spawnDecorations(Chunk &chunk) override;

private:
  TreeSpawner m_tree_spawner;
};
