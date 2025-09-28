#pragma once

#include "biome.hpp"
#include "tree_spawner.hpp"

class DesertBiome : public Biome {
public:
  DesertBiome();
  void generateTerrain(Chunk &chunk) override;
  void spawnDecorations(Chunk &chunk) override;

private:
  TreeSpawner m_palm_tree_spawner;
};
