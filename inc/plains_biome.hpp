#pragma once

#include "biome.hpp"
#include "tree_spawner.hpp"

class PlainsBiome : public Biome {
public:
  PlainsBiome();
  void generateTerrain(Chunk &chunk) override;
  void spawnDecorations(Chunk &chunk) override;

private:
  TreeSpawner m_tree_spawner;
};
