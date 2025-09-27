#pragma once

#include <memory>
#include "biome.hpp"

enum class BiomeType {
    PLAINS,
    FOREST,
    DESERT,
    OCEAN
};

class BiomeManager {
public:
    static std::unique_ptr<Biome> createBiome(BiomeType type);
    static BiomeType getBiomeForChunk(int cx, int cz);
};
