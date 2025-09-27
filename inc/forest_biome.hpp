#pragma once

#include "biome.hpp"

class ForestBiome : public Biome {
public:
    void generateTerrain(Chunk& chunk) override;
    void spawnDecorations(Chunk& chunk) override;
};
