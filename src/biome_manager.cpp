#include "biome_manager.hpp"
#include "plains_biome.hpp"
#include "forest_biome.hpp"
#include "desert_biome.hpp"
#include <glm/gtc/noise.hpp>

std::unique_ptr<Biome> BiomeManager::createBiome(BiomeType type) {
    switch (type) {
        case BiomeType::PLAINS:
            return std::make_unique<PlainsBiome>();
        case BiomeType::FOREST:
            return std::make_unique<ForestBiome>();
        case BiomeType::DESERT:
            return std::make_unique<DesertBiome>();
    }
    return std::make_unique<PlainsBiome>(); // Default to plains
}

BiomeType BiomeManager::getBiomeForChunk(int cx, int cz) {
    glm::vec2 pos(cx * 0.01f, cz * 0.01f);
    double noise = glm::perlin(pos) * 0.5 + 0.5; // [0, 1]

    if (noise < 0.33) {
        return BiomeType::DESERT;
    } else if (noise < 0.66) {
        return BiomeType::PLAINS;
    } else {
        return BiomeType::FOREST;
    }
}
