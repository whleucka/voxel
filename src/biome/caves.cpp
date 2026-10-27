#include "biome/caves.hpp"
#include "biome/biome.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "core/settings.hpp"
#include "util/worldgen_rng.hpp"
#include <algorithm>
#include <array>
#include <glm/gtc/noise.hpp>
#include <random>

namespace caves {

namespace {

// Two noise fields; a tunnel runs where both are near zero, which traces
// long, winding worms instead of blobs. y is stretched so tunnels wander
// sideways more than up and down.
constexpr float kFrequency = 0.03f;
constexpr float kVerticalStretch = 1.6f;
constexpr float kRadius = 0.085f; // in noise units: tunnels ~3-5 blocks across
const glm::vec3 kSecondField{113.7f, 51.1f, 87.3f};

// Caves come in systems: a low-frequency mask leaves most ground solid, and
// tunnels narrow to nothing at a system's edge instead of being cut off.
constexpr float kRegionFrequency = 0.011f;
const glm::vec2 kRegionOffset{71.3f, 19.7f};
constexpr float kRegionEdge = 0.05f, kRegionCore = 0.25f;

// What grows inside: depths are below sea level, odds are one in N.
constexpr int kPoolDepth = 25;
constexpr float kPoolFrequency = 0.12f;
constexpr float kPoolThreshold = 0.42f;
constexpr int kCrystalDepth = 15;
constexpr unsigned kCrystalOdds = 60;
constexpr unsigned kWallOreOdds = 20;
constexpr int kGemDepth = 30;

// The fields are sampled every kStep blocks and blended in between, which
// keeps a chunk cheap. Lattice points are world-aligned, so two chunks agree
// exactly on the shared values at their border.
constexpr int kStep = 4;
constexpr int kLatticeX = kChunkWidth / kStep + 1;
constexpr int kLatticeY = kChunkHeight / kStep + 1;
constexpr int kLatticeZ = kChunkDepth / kStep + 1;

struct Fields { float a, b; };

glm::vec3 noisePos(glm::ivec3 w) {
  return glm::vec3(w.x + g_settings.noise_offset.x, w.y * kVerticalStretch,
                   w.z + g_settings.noise_offset.y) * kFrequency;
}

Fields fieldsAt(glm::ivec3 lattice_world) {
  glm::vec3 p = noisePos(lattice_world);
  return {glm::perlin(p), glm::perlin(p + kSecondField)};
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Blends the eight lattice corners around a block; `c` is indexed
// [dx + 2 * (dy + 2 * dz)].
Fields blend(const std::array<Fields, 8> &c, float fx, float fy, float fz) {
  auto one = [&](float Fields::*f) {
    float x00 = lerp(c[0].*f, c[1].*f, fx), x10 = lerp(c[2].*f, c[3].*f, fx);
    float x01 = lerp(c[4].*f, c[5].*f, fx), x11 = lerp(c[6].*f, c[7].*f, fx);
    return lerp(lerp(x00, x10, fy), lerp(x01, x11, fy), fz);
  };
  return {one(&Fields::a), one(&Fields::b)};
}

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

int surfaceAt(int wx, int wz) {
  return std::min(Biome::getHeight({wx, wz}), kChunkHeight - 1);
}

// Everything about a column that decides how high its caves may go.
struct Column {
  int surface = 0; // blocks y < surface are ground
  int ceiling = 0; // caves stay below this y
  float radius = 0.0f;
};

Column columnAt(int wx, int wz, int (*height)(int, int, const void *), const void *ctx) {
  Column c;
  c.surface = height(wx, wz, ctx);
  // Water lies on any column at or below sea level. Caves may only open to
  // the sky where all the ground around is high and dry; anywhere else they
  // keep a roof of rock under the lowest neighbour, so no cave touches the
  // sea or a lake.
  int lowest = c.surface;
  for (int dx = -1; dx <= 1; ++dx)
    for (int dz = -1; dz <= 1; ++dz)
      if (dx || dz) lowest = std::min(lowest, height(wx + dx, wz + dz, ctx));
  c.ceiling = lowest > kSeaLevel + kOpenAbove ? c.surface : lowest - kSafeRoof;

  float mask = glm::perlin((glm::vec2(wx, wz) + g_settings.noise_offset) * kRegionFrequency +
                           kRegionOffset);
  c.radius = kRadius * glm::smoothstep(kRegionEdge, kRegionCore, mask);
  return c;
}

bool tunnel(const Fields &f, const Column &c, int y) {
  return y >= kFloor && y < c.ceiling && f.a * f.a + f.b * f.b < c.radius * c.radius;
}

int directHeight(int wx, int wz, const void *) { return surfaceAt(wx, wz); }

// Heights for a chunk and a one-block rim around it, computed once.
struct Heights {
  glm::ivec2 origin; // world x, z of index (0, 0)
  std::array<int, (kChunkWidth + 2) * (kChunkDepth + 2)> h{};
  int &at(int wx, int wz) {
    return h[(wx - origin.x) + (kChunkWidth + 2) * (wz - origin.y)];
  }
};

int cachedHeight(int wx, int wz, const void *ctx) {
  return const_cast<Heights *>(static_cast<const Heights *>(ctx))->at(wx, wz);
}

BlockType wallOre(std::mt19937 &rng, bool deep) {
  unsigned roll = rng() % 10;
  if (deep) {
    if (roll == 0) return BlockType::DIAMOND_ORE;
    if (roll <= 3) return BlockType::EMERALD_ORE;
    if (roll <= 5) return BlockType::RUBY_ORE;
    if (roll <= 7) return BlockType::GOLD_ORE;
    return BlockType::IRON_ORE;
  }
  if (roll <= 3) return BlockType::COAL_ORE;
  if (roll <= 6) return BlockType::COPPER_ORE;
  return BlockType::IRON_ORE;
}

} // namespace

bool isTunnel(glm::ivec3 w) {
  int gx = floorDiv(w.x, kStep), gy = floorDiv(w.y, kStep), gz = floorDiv(w.z, kStep);
  std::array<Fields, 8> corners;
  for (int dz = 0; dz < 2; ++dz)
    for (int dy = 0; dy < 2; ++dy)
      for (int dx = 0; dx < 2; ++dx)
        corners[dx + 2 * (dy + 2 * dz)] =
            fieldsAt({(gx + dx) * kStep, (gy + dy) * kStep, (gz + dz) * kStep});
  Fields f = blend(corners, float(w.x - gx * kStep) / kStep, float(w.y - gy * kStep) / kStep,
                   float(w.z - gz * kStep) / kStep);
  return tunnel(f, columnAt(w.x, w.z, directHeight, nullptr), w.y);
}

void carve(Chunk &chunk, Surfaces &surfaces) {
  const int wx0 = chunk.getPos().x * kChunkWidth;
  const int wz0 = chunk.getPos().y * kChunkDepth;

  Heights heights{{wx0 - 1, wz0 - 1}};
  for (int z = -1; z <= kChunkDepth; ++z)
    for (int x = -1; x <= kChunkWidth; ++x) heights.at(wx0 + x, wz0 + z) = surfaceAt(wx0 + x, wz0 + z);
  for (int z = 0; z < kChunkDepth; ++z)
    for (int x = 0; x < kChunkWidth; ++x) surfaces[x + kChunkWidth * z] = heights.at(wx0 + x, wz0 + z);

  std::array<Column, kChunkWidth * kChunkDepth> columns;
  bool any = false;
  for (int z = 0; z < kChunkDepth; ++z)
    for (int x = 0; x < kChunkWidth; ++x) {
      Column &c = columns[x + kChunkWidth * z] =
          columnAt(wx0 + x, wz0 + z, cachedHeight, &heights);
      any |= c.radius > 0.0f && c.ceiling > kFloor;
    }
  if (!any) return; // outside every cave system: skip the 3D noise

  static_assert(kChunkWidth % kStep == 0 && kChunkHeight % kStep == 0);
  std::vector<Fields> lattice(kLatticeX * kLatticeY * kLatticeZ);
  auto node = [&](int lx, int ly, int lz) -> Fields & {
    return lattice[lx + kLatticeX * (lz + kLatticeZ * ly)];
  };
  for (int ly = 0; ly < kLatticeY; ++ly)
    for (int lz = 0; lz < kLatticeZ; ++lz)
      for (int lx = 0; lx < kLatticeX; ++lx)
        node(lx, ly, lz) = fieldsAt({wx0 + lx * kStep, ly * kStep, wz0 + lz * kStep});

  for (int y = kFloor; y < kChunkHeight; ++y) {
    const int ly = y / kStep;
    const float fy = float(y - ly * kStep) / kStep;
    for (int z = 0; z < kChunkDepth; ++z) {
      const int lz = z / kStep;
      const float fz = float(z - lz * kStep) / kStep;
      for (int x = 0; x < kChunkWidth; ++x) {
        const Column &c = columns[x + kChunkWidth * z];
        if (y >= c.ceiling || c.radius <= 0.0f) continue;
        const int lx = x / kStep;
        std::array<Fields, 8> corners;
        for (int dz = 0; dz < 2; ++dz)
          for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx)
              corners[dx + 2 * (dy + 2 * dz)] = node(lx + dx, ly + dy, lz + dz);
        Fields f = blend(corners, float(x - lx * kStep) / kStep, fy, fz);
        if (tunnel(f, c, y)) chunk.at(x, y, z) = BlockType::AIR;
      }
    }
  }
}

void decorate(Chunk &chunk, const Surfaces &surface) {
  const int wx0 = chunk.getPos().x * kChunkWidth;
  const int wz0 = chunk.getPos().y * kChunkDepth;
  const int top = *std::max_element(surface.begin(), surface.end());

  auto ground = [&](int x, int z) { return surface[x + kChunkWidth * z]; };
  auto caveAir = [&](int x, int y, int z) {
    return x >= 0 && x < kChunkWidth && z >= 0 && z < kChunkDepth && y >= 0 &&
           y < ground(x, z) && chunk.at(x, y, z) == BlockType::AIR;
  };
  auto rockAt = [&](int x, int y, int z) {
    if (x < 0 || x >= kChunkWidth || z < 0 || z >= kChunkDepth || y < 0 || y >= kChunkHeight)
      return false;
    BlockType b = chunk.at(x, y, z);
    return b != BlockType::AIR && !isLiquid(b) && b != BlockType::GLOW_CRYSTAL;
  };

  // Its own stream, so the rest of generation draws exactly what it always did.
  uint64_t h = worldgen::mix(g_settings.world_seed ^ 0xCA7Eull);
  h = worldgen::mix(h ^ static_cast<uint32_t>(chunk.getPos().x));
  h = worldgen::mix(h ^ static_cast<uint32_t>(chunk.getPos().y));
  std::mt19937 rng(static_cast<uint32_t>(h ^ (h >> 32)));

  for (int y = kFloor; y < top; ++y)
    for (int z = 0; z < kChunkDepth; ++z)
      for (int x = 0; x < kChunkWidth; ++x) {
        BlockType &b = chunk.at(x, y, z);
        if (caveAir(x, y, z)) {
          const bool floor = rockAt(x, y - 1, z);
          // Shallow pools on low floors: one block of water on rock, so
          // nobody's head goes under.
          if (floor && y < kSeaLevel - kPoolDepth) {
            glm::vec2 p = (glm::vec2(wx0 + x, wz0 + z) + g_settings.noise_offset) *
                              kPoolFrequency + glm::vec2(y * 0.37f);
            if (glm::perlin(p) > kPoolThreshold) {
              b = BlockType::WATER;
              continue;
            }
          }
          // Glow crystals on floors and ceilings, deep enough to be dark.
          if ((floor || rockAt(x, y + 1, z)) && y < kSeaLevel - kCrystalDepth &&
              rng() % kCrystalOdds == 0)
            b = BlockType::GLOW_CRYSTAL;
        } else if (b == BlockType::STONE || b == BlockType::COBBLESTONE) {
          // Gems in the walls, and better ones deeper down.
          const bool wall = caveAir(x + 1, y, z) || caveAir(x - 1, y, z) ||
                            caveAir(x, y + 1, z) || caveAir(x, y - 1, z) ||
                            caveAir(x, y, z + 1) || caveAir(x, y, z - 1);
          if (wall && rng() % kWallOreOdds == 0)
            b = wallOre(rng, y < kSeaLevel - kGemDepth);
        }
      }
}

void placeTreasure(Chunk &chunk, const Surfaces &surface) {
  uint64_t h = worldgen::mix(g_settings.world_seed ^ 0x7EA5u);
  h = worldgen::mix(h ^ static_cast<uint32_t>(chunk.getPos().x));
  h = worldgen::mix(h ^ static_cast<uint32_t>(chunk.getPos().y));
  std::mt19937 rng(static_cast<uint32_t>(h ^ (h >> 32)));
  if (rng() % kTreasureOdds != 0) return;

  auto rock = [&](int x, int y, int z) {
    BlockType b = chunk.at(x, y, z);
    return b != BlockType::AIR && !isLiquid(b) && b != BlockType::GLOW_CRYSTAL;
  };
  // A nook: cave air on a rock floor, under the ground, with rock on at
  // least three sides. Inside the chunk, so it never depends on a neighbour.
  std::vector<glm::ivec3> nooks;
  for (int z = 1; z < kChunkDepth - 1; ++z)
    for (int x = 1; x < kChunkWidth - 1; ++x) {
      const int ground = surface[x + kChunkWidth * z];
      for (int y = kFloor; y < std::min(ground, kSeaLevel - kTreasureDepth); ++y) {
        if (chunk.at(x, y, z) != BlockType::AIR || !rock(x, y - 1, z)) continue;
        int sides = rock(x + 1, y, z) + rock(x - 1, y, z) + rock(x, y, z + 1) + rock(x, y, z - 1);
        if (sides >= 3) nooks.push_back({x, y, z});
      }
    }
  if (nooks.empty()) return;
  glm::ivec3 at = nooks[rng() % nooks.size()];
  chunk.at(at.x, at.y, at.z) = BlockType::TREASURE_CHEST;
}

} // namespace caves
