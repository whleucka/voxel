#include "chunk/chunk.hpp"
#include "block/block_data.hpp"
#include "render/texture_manager.hpp"
#include "biome/biome_manager.hpp"
#include "core/constants.hpp"
#include <queue>

Chunk::Chunk(int x, int z)
    : pos({x, z}),
      blocks(kChunkWidth * kChunkHeight * kChunkDepth, BlockType::AIR),
      skylight(kChunkWidth * kChunkHeight * kChunkDepth, 0) {}



void Chunk::init() {
  BiomeType biome_type = BiomeManager::getBiomeForChunk(pos[0], pos[1]);
  std::unique_ptr<Biome> biome = BiomeManager::createBiome(biome_type);
  biome->generateTerrain(*this);
  biome->generateMinerals(*this);
  biome->fillWater(*this);
  biome->spawnDecorations(*this);
  computeSkyLight();
}

void Chunk::buildMeshData(World* world, TextureManager& texture_manager) {
  std::lock_guard lock(data_mutex);
  mesh.generateCPU(world, *this, texture_manager); // CPU-only
}

void Chunk::uploadGPU() {
  mesh.upload();
}

BlockType &Chunk::at(int x, int y, int z) {
  return blocks[x + kChunkWidth * (z + kChunkDepth * y)];
}

const BlockType &Chunk::at(int x, int y, int z) const {
  return blocks[x + kChunkWidth * (z + kChunkDepth * y)];
}

BlockType Chunk::safeAt(int x, int y, int z) const {
  if (x < 0 || x >= kChunkWidth || y < 0 || y >= kChunkHeight || z < 0 ||
      z >= kChunkDepth) {
    return BlockType::AIR;
  }
  return at(x, y, z);
}

glm::mat4 Chunk::getModelMatrix() const {
  return glm::translate(glm::mat4(1.0f),
                        glm::vec3(pos[0] * kChunkWidth, 0, pos[1] * kChunkDepth));
}

uint8_t Chunk::getSkyLight(int x, int y, int z) const {
  return skylight[x + kChunkWidth * (z + kChunkDepth * y)];
}

uint8_t Chunk::safeSkyLight(int x, int y, int z) const {
  if (x < 0 || x >= kChunkWidth || y < 0 || y >= kChunkHeight ||
      z < 0 || z >= kChunkDepth)
    return 0;
  return getSkyLight(x, y, z);
}

void Chunk::computeSkyLight() {
  std::fill(skylight.begin(), skylight.end(), 0);

  using Queue = std::queue<std::tuple<int,int,int>>;
  Queue queue;

  // Phase 1 — top-down column scan.
  // Sky light (value 15) propagates straight down through transparent blocks.
  // Semi-transparent blocks (leaves, water) absorb some levels.
  for (int z = 0; z < kChunkDepth; ++z) {
    for (int x = 0; x < kChunkWidth; ++x) {
      uint8_t light = 15;
      for (int y = kChunkHeight - 1; y >= 0 && light > 0; --y) {
        BlockType b = at(x, y, z);
        uint8_t opacity = skyLightOpacity(b);
        if (opacity >= 15) break; // fully opaque — stop column
        if (opacity > 0) {
          light = (light > opacity) ? light - opacity : 0;
        }
        int idx = x + kChunkWidth * (z + kChunkDepth * y);
        skylight[idx] = light;
        if (light > 0)
          queue.push({x, y, z});
      }
    }
  }

  // Phase 2 — BFS flood fill.
  // Spreads light horizontally (and upward) under overhangs / into cave mouths.
  static constexpr int DX[6] = {-1, 1,  0, 0,  0, 0};
  static constexpr int DY[6] = { 0, 0, -1, 1,  0, 0};
  static constexpr int DZ[6] = { 0, 0,  0, 0, -1, 1};

  while (!queue.empty()) {
    auto [x, y, z] = queue.front();
    queue.pop();
    uint8_t light = skylight[x + kChunkWidth * (z + kChunkDepth * y)];
    if (light <= 1) continue;

    for (int d = 0; d < 6; ++d) {
      int nx = x + DX[d], ny = y + DY[d], nz = z + DZ[d];
      if (nx < 0 || nx >= kChunkWidth || ny < 0 || ny >= kChunkHeight ||
          nz < 0 || nz >= kChunkDepth) continue;

      BlockType nb = at(nx, ny, nz);
      uint8_t opacity = skyLightOpacity(nb);
      if (opacity >= 15) continue;

      // Each hop decays by 1; semi-transparent blocks decay by 1+opacity
      uint8_t cost = 1 + opacity;
      uint8_t newLight = (light > cost) ? static_cast<uint8_t>(light - cost) : 0;
      if (newLight == 0) continue;

      int idx = nx + kChunkWidth * (nz + kChunkDepth * ny);
      if (newLight > skylight[idx]) {
        skylight[idx] = newLight;
        queue.push({nx, ny, nz});
      }
    }
  }
}
