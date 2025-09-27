#pragma once

#include "biome.hpp"

class PlainsBiome : public Biome {
public:
    void generateTerrain(Chunk& chunk) override;
    void spawnDecorations(Chunk& chunk) override;
};
