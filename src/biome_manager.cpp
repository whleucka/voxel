#include "biome_manager.hpp"
#include "plains_biome.hpp"
#include "forest_biome.hpp"
#include "desert_biome.hpp"
#include "ocean_biome.hpp"
#include "terrain_generator.hpp"
#include "chunk.hpp"
#include <glm/gtc/noise.hpp>

const int SEA_LEVEL = 44;

std::unique_ptr<Biome> BiomeManager::createBiome(BiomeType type) {
    switch (type) {
        case BiomeType::PLAINS:
            return std::make_unique<PlainsBiome>();
        case BiomeType::FOREST:
            return std::make_unique<ForestBiome>();
        case BiomeType::DESERT:
            return std::make_unique<DesertBiome>();
        case BiomeType::OCEAN:
            return std::make_unique<OceanBiome>();
    }
    return std::make_unique<PlainsBiome>(); // Default to plains
}

BiomeType BiomeManager::getBiomeForChunk(int cx, int cz) {
    float chunk_center_x = (cx + 0.5f) * Chunk::W;
    float chunk_center_z = (cz + 0.5f) * Chunk::L;

    float biome_noise;
    int center_height = TerrainGenerator::getHeight(chunk_center_x, chunk_center_z, &biome_noise);

    if (center_height < SEA_LEVEL) {
        return BiomeType::OCEAN;
    }

    if (center_height < 80) { // Lowlands
        if (biome_noise < 0.33) {
            return BiomeType::DESERT;
        } else {
            return BiomeType::PLAINS;
        }
    } else { // Highlands
        if (biome_noise < 0.5) {
            return BiomeType::PLAINS; // Grassy hills
        } else {
            return BiomeType::FOREST; // Mountain forests
        }
    }
}
