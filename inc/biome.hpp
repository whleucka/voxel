#pragma once

#include "chunk.hpp"

class Biome {
public:
    virtual void generateTerrain(Chunk& chunk) = 0;
    virtual void spawnDecorations(Chunk& chunk) = 0;
    virtual ~Biome() {}
};
