#pragma once

#include "chunk/chunk.hpp"

class Biome {
public:
    void generateTerrain(Chunk& chunk);
    void fillWater(Chunk& chunk);
    static int getHeight(glm::vec2 pos, float *biome_noise_out = nullptr);
    virtual BlockType generateInternalBlock(int x, int y, int z);
    virtual void generateMinerals(Chunk& chunk);
    virtual BlockType generateTopBlock(int y) = 0;
    virtual void spawnDecorations(Chunk& chunk) = 0;
    virtual ~Biome() {}
};

