#include "chunk/chunk.hpp"
#include "block/block_data.hpp"
#include "render/texture_manager.hpp"
#include "biome/biome_manager.hpp"
#include "biome/caves.hpp"
#include "biome/plants.hpp"
#include "core/constants.hpp"
#include "core/settings.hpp"
#include "util/worldgen_rng.hpp"
#include <queue>

Chunk::Chunk(int x, int z)
    : pos({x, z}),
      blocks(kChunkWidth * kChunkHeight * kChunkDepth, BlockType::AIR),
      skylight(kChunkWidth * kChunkHeight * kChunkDepth, 0),
      blocklight(kChunkWidth * kChunkHeight * kChunkDepth, 0) {}



void Chunk::init() {
  // Everything random below draws from this, so the chunk is a pure function
  // of (seed, position).
  worldgen::seedChunk(g_settings.world_seed, pos[0], pos[1]);
  BiomeType biome_type = BiomeManager::getBiomeForChunk(pos[0], pos[1]);
  std::unique_ptr<Biome> biome = BiomeManager::createBiome(biome_type);
  const bool with_caves = g_settings.worldgen >= worldgen::kCaves;
  biome->generateTerrain(*this);
  caves::Surfaces surfaces;
  if (with_caves) caves::carve(*this, surfaces);
  biome->generateMinerals(*this);
  biome->fillWater(*this);
  biome->spawnDecorations(*this);
  if (with_caves) caves::decorate(*this, surfaces);
  if (g_settings.worldgen >= worldgen::kChests) caves::placeTreasure(*this, surfaces);
  if (g_settings.worldgen >= worldgen::kPockets) caves::placeGemPockets(*this, surfaces);
  if (g_settings.worldgen >= worldgen::kPools) caves::placePools(*this, surfaces);
  if (g_settings.worldgen >= worldgen::kPlants) plants::place(*this, biome_type, surfaces);
  computeSkyLight();
  computeBlockLight();
}

void Chunk::buildMeshData(World* world, TextureManager& texture_manager) {
  std::lock_guard lock(mesh_mutex);
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

void Chunk::setBlockLinear(uint32_t index, BlockType type) {
  if (index < blocks.size())
    blocks[index] = type;
}

void Chunk::setBlockLightLinear(uint32_t index, uint8_t value) {
  if (index < blocklight.size())
    blocklight[index] = value;
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
        skylight[x + kChunkWidth * (z + kChunkDepth * y)] = light;
      }
    }
  }

  // Phase 2 — flood fill.
  // Spreads light horizontally (and upward) under overhangs / into cave mouths,
  // from the cells that would brighten a neighbour (most open sky wouldn't).
  LightQueue queue;
  for (int y = 0; y < kChunkHeight; ++y)
    for (int z = 0; z < kChunkDepth; ++z)
      for (int x = 0; x < kChunkWidth; ++x) {
        const uint32_t i = x + kChunkWidth * (z + kChunkDepth * y);
        if (skylight[i] > 1 && brightensNeighbour(skylight, x, y, z)) queue.push_back(i);
      }
  floodLight(skylight, queue);
}

uint8_t Chunk::getBlockLight(int x, int y, int z) const {
  return blocklight[x + kChunkWidth * (z + kChunkDepth * y)];
}

uint8_t Chunk::safeBlockLight(int x, int y, int z) const {
  if (x < 0 || x >= kChunkWidth || y < 0 || y >= kChunkHeight ||
      z < 0 || z >= kChunkDepth)
    return 0;
  return getBlockLight(x, y, z);
}

void Chunk::computeBlockLight() {
  std::fill(blocklight.begin(), blocklight.end(), 0);

  // Seed the BFS with every emissive block at its emission level. This is the
  // intra-chunk pass; light crossing into neighbours is the World's job (see
  // World::spreadBorderLight and World::relightBlockRegion).
  LightQueue queue;
  emitter_count = 0;
  for (int y = 0; y < kChunkHeight; ++y)
    for (int z = 0; z < kChunkDepth; ++z)
      for (int x = 0; x < kChunkWidth; ++x) {
        uint8_t emit = blockLightEmission(at(x, y, z));
        if (emit > 0) {
          ++emitter_count;
          const uint32_t i = x + kChunkWidth * (z + kChunkDepth * y);
          blocklight[i] = emit;
          queue.push_back(i);
        }
      }
  floodLight(blocklight, queue);
}

// How much light `light` at (x, y, z) gives its neighbour `d` (0 if none):
// opaque blocks stop it, semi-transparent ones cost extra.
static constexpr int DX[6] = {-1, 1, 0, 0, 0, 0};
static constexpr int DY[6] = {0, 0, -1, 1, 0, 0};
static constexpr int DZ[6] = {0, 0, 0, 0, -1, 1};

bool Chunk::brightensNeighbour(const std::vector<uint8_t> &light, int x, int y, int z) const {
  const uint8_t l = light[x + kChunkWidth * (z + kChunkDepth * y)];
  for (int d = 0; d < 6; ++d) {
    int nx = x + DX[d], ny = y + DY[d], nz = z + DZ[d];
    if (nx < 0 || nx >= kChunkWidth || ny < 0 || ny >= kChunkHeight ||
        nz < 0 || nz >= kChunkDepth) continue;
    uint8_t opacity = skyLightOpacity(at(nx, ny, nz));
    if (opacity >= 15) continue;
    uint8_t cost = 1 + opacity;
    if (l > cost && l - cost > light[nx + kChunkWidth * (nz + kChunkDepth * ny)]) return true;
  }
  return false;
}

void Chunk::floodLight(std::vector<uint8_t> &light, LightQueue &queue) {
  for (size_t head = 0; head < queue.size(); ++head) {
    const uint32_t i = queue[head];
    const int x = i % kChunkWidth, z = i / kChunkWidth % kChunkDepth,
              y = i / (kChunkWidth * kChunkDepth);
    uint8_t l = light[i];
    if (l <= 1) continue;

    for (int d = 0; d < 6; ++d) {
      int nx = x + DX[d], ny = y + DY[d], nz = z + DZ[d];
      if (nx < 0 || nx >= kChunkWidth || ny < 0 || ny >= kChunkHeight ||
          nz < 0 || nz >= kChunkDepth) continue;

      // Opaque blocks stop light; semi-transparent ones cost extra.
      uint8_t opacity = skyLightOpacity(at(nx, ny, nz));
      if (opacity >= 15) continue;

      uint8_t cost = 1 + opacity;
      uint8_t newLight = (l > cost) ? static_cast<uint8_t>(l - cost) : 0;
      if (newLight == 0) continue;

      const uint32_t n = nx + kChunkWidth * (nz + kChunkDepth * ny);
      if (newLight > light[n]) {
        light[n] = newLight;
        queue.push_back(n);
      }
    }
  }
}

glm::ivec2 Chunk::faceCell(int face, int i) {
  switch (face) {
    case 0:  return {0, i};
    case 1:  return {kChunkWidth - 1, i};
    case 2:  return {i, 0};
    default: return {i, kChunkDepth - 1};
  }
}

Chunk::BorderLight Chunk::borderLight(int face) const {
  BorderLight out;
  for (int y = 0; y < kChunkHeight; ++y)
    for (int i = 0; i < kChunkWidth; ++i) {
      glm::ivec2 c = faceCell(face, i);
      out[i + kChunkWidth * y] = getBlockLight(c.x, y, c.y);
    }
  return out;
}

bool Chunk::acceptBorderLight(int face, const BorderLight &incoming) {
  LightQueue queue;
  for (int y = 0; y < kChunkHeight; ++y)
    for (int i = 0; i < kChunkWidth; ++i) {
      uint8_t from = incoming[i + kChunkWidth * y];
      if (from <= 1) continue;
      glm::ivec2 c = faceCell(face, i);
      uint8_t opacity = skyLightOpacity(at(c.x, y, c.y));
      if (opacity >= 15) continue;
      uint8_t cost = 1 + opacity;
      if (from <= cost) continue;
      uint8_t light = static_cast<uint8_t>(from - cost);
      uint8_t &here = blocklight[c.x + kChunkWidth * (c.y + kChunkDepth * y)];
      if (light > here) {
        here = light;
        queue.push_back(c.x + kChunkWidth * (c.y + kChunkDepth * y));
      }
    }
  if (queue.empty()) return false;
  floodLight(blocklight, queue);
  return true;
}
