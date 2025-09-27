#pragma once

class TerrainGenerator {
public:
    static int getHeight(float x, float z, float* biome_noise_out = nullptr);
};
