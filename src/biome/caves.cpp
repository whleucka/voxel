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
#include <unordered_map>

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
    // From version 4 the gems are in pockets instead.
    if (g_settings.worldgen >= worldgen::kPockets && roll <= 5) return BlockType::IRON_ORE;
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

// The floor sits closer to the centre than the roof, so a room is a flattish
// dome rather than a ball, and its edge wobbles so it doesn't look turned.
constexpr float kRoomFloor = 0.55f; // of the height above the centre
constexpr float kRoomWobble = 0.25f;
constexpr float kRoomWobbleFrequency = 0.18f;
constexpr int kRoomTries = 96; // spots tried for a tunnel to grow from

} // namespace

namespace {

std::optional<Room> findRoom(int sx, int sz) {
  uint64_t h = worldgen::mix(g_settings.world_seed ^ 0x2003ull);
  h = worldgen::mix(h ^ static_cast<uint32_t>(sx));
  h = worldgen::mix(h ^ static_cast<uint32_t>(sz));
  std::mt19937 rng(static_cast<uint32_t>(h ^ (h >> 32)));
  if (rng() % 100 >= kRoomOdds) return std::nullopt;

  Room room;
  room.radius = {kRoomMinRadius + float(rng() % (kRoomMaxRadius - kRoomMinRadius + 1)),
                 kRoomMinHalfHeight + float(rng() % (kRoomMaxHalfHeight - kRoomMinHalfHeight + 1)),
                 kRoomMinRadius + float(rng() % (kRoomMaxRadius - kRoomMinRadius + 1))};
  // Grow it from a tunnel, so it joins the caves: try a few spots, keeping
  // clear of the square's edge so neighbouring rooms don't merge.
  const int margin = kRoomMaxRadius + 2;
  for (int tries = 0; tries < kRoomTries; ++tries) {
    glm::ivec3 p{sx * kRoomCell + margin + int(rng() % (kRoomCell - 2 * margin)),
                 kFloor + int(room.radius.y * kRoomFloor) + 1 + int(rng() % (kSeaLevel - kFloor)),
                 sz * kRoomCell + margin + int(rng() % (kRoomCell - 2 * margin))};
    if (isTunnel(p)) {
      room.centre = p;
      return room;
    }
  }
  return std::nullopt;
}

} // namespace

// Every chunk near a square asks for its room, so each worker thread keeps
// the answers (for this seed) rather than searching for tunnels again.
std::optional<Room> roomIn(int sx, int sz) {
  struct Cache {
    uint32_t seed = 0;
    glm::vec2 offset{0.0f};
    std::unordered_map<int64_t, std::optional<Room>> rooms;
  };
  thread_local Cache cache;
  if (cache.seed != g_settings.world_seed || cache.offset != g_settings.noise_offset ||
      cache.rooms.size() > 4096) {
    cache = {g_settings.world_seed, g_settings.noise_offset, {}};
  }
  const int64_t key = (static_cast<int64_t>(sx) << 32) ^ static_cast<uint32_t>(sz);
  auto it = cache.rooms.find(key);
  if (it == cache.rooms.end()) it = cache.rooms.emplace(key, findRoom(sx, sz)).first;
  return it->second;
}

bool inRoom(const Room &room, glm::ivec3 w) {
  glm::vec3 d = glm::vec3(w - room.centre);
  glm::vec3 r = room.radius;
  if (d.y < 0) r.y *= kRoomFloor;
  float wobble = 1.0f + kRoomWobble * glm::perlin((glm::vec3(w) + glm::vec3(g_settings.noise_offset.x, 0.0f,
                                                                               g_settings.noise_offset.y)) *
                                                  kRoomWobbleFrequency);
  glm::vec3 n = d / r;
  return glm::dot(n, n) < wobble * wobble;
}

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

namespace {

// The rooms whose squares reach into this chunk, carved under each column's
// roof, like the tunnels.
void carveRooms(Chunk &chunk, const std::array<Column, kChunkWidth * kChunkDepth> &columns) {
  const int wx0 = chunk.getPos().x * kChunkWidth;
  const int wz0 = chunk.getPos().y * kChunkDepth;
  for (int sx = floorDiv(wx0, kRoomCell) - 1; sx <= floorDiv(wx0 + kChunkWidth - 1, kRoomCell) + 1; ++sx)
    for (int sz = floorDiv(wz0, kRoomCell) - 1; sz <= floorDiv(wz0 + kChunkDepth - 1, kRoomCell) + 1; ++sz) {
      std::optional<Room> room = roomIn(sx, sz);
      if (!room) continue;
      const glm::ivec3 reach = glm::ivec3(glm::ceil(room->radius * (1.0f + kRoomWobble))) + 1;
      const glm::ivec3 lo = room->centre - reach, hi = room->centre + reach;
      for (int z = std::max(lo.z, wz0); z <= std::min(hi.z, wz0 + kChunkDepth - 1); ++z)
        for (int x = std::max(lo.x, wx0); x <= std::min(hi.x, wx0 + kChunkWidth - 1); ++x) {
          const Column &c = columns[(x - wx0) + kChunkWidth * (z - wz0)];
          for (int y = std::max(lo.y, kFloor); y <= std::min(hi.y, c.ceiling - 1); ++y)
            if (inRoom(*room, {x, y, z})) chunk.at(x - wx0, y, z - wz0) = BlockType::AIR;
        }
    }
}

} // namespace

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
  if (g_settings.worldgen >= worldgen::kRooms) carveRooms(chunk, columns);
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

void placeGemPockets(Chunk &chunk, const Surfaces &surface) {
  uint64_t h = worldgen::mix(g_settings.world_seed ^ 0x6E35u);
  h = worldgen::mix(h ^ static_cast<uint32_t>(chunk.getPos().x));
  h = worldgen::mix(h ^ static_cast<uint32_t>(chunk.getPos().y));
  std::mt19937 rng(static_cast<uint32_t>(h ^ (h >> 32)));

  auto host = [&](glm::ivec3 p) {
    if (p.x < 0 || p.x >= kChunkWidth || p.z < 0 || p.z >= kChunkDepth || p.y < kPocketFloor ||
        p.y >= kSeaLevel - kPocketDepth)
      return false;
    BlockType b = chunk.at(p.x, p.y, p.z);
    return b == BlockType::STONE || b == BlockType::COBBLESTONE || b == BlockType::COAL_ORE ||
           b == BlockType::COPPER_ORE || b == BlockType::IRON_ORE;
  };
  // Cave air: open, and under the ground (not the sky over a low valley).
  auto caveAir = [&](glm::ivec3 p) {
    if (p.x < 0 || p.x >= kChunkWidth || p.z < 0 || p.z >= kChunkDepth) return false;
    BlockType b = chunk.at(p.x, p.y, p.z);
    return (b == BlockType::AIR || b == BlockType::WATER || b == BlockType::GLOW_CRYSTAL) &&
           p.y < surface[p.x + kChunkWidth * p.z];
  };
  const glm::ivec3 kSides[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

  // Spots in a cave wall, so some pockets can be found by exploring.
  std::vector<glm::ivec3> walls;
  for (int y = kPocketFloor; y < kSeaLevel - kPocketDepth; ++y)
    for (int z = 0; z < kChunkDepth; ++z)
      for (int x = 0; x < kChunkWidth; ++x) {
        glm::ivec3 p{x, y, z};
        if (!host(p)) continue;
        for (glm::ivec3 d : kSides)
          if (caveAir(p + d)) {
            walls.push_back(p);
            break;
          }
      }

  unsigned roll = rng() % 100;
  int pockets = roll < kPocketNoneOdds ? 0 : roll < kPocketNoneOdds + kPocketOneOdds ? 1 : 2;
  for (int i = 0; i < pockets; ++i) {
    glm::ivec3 at;
    if (!walls.empty() && rng() % 100 < kPocketInWallOdds) {
      at = walls[rng() % walls.size()];
    } else {
      at = {static_cast<int>(rng() % kChunkWidth),
            kPocketFloor + static_cast<int>(rng() % (kSeaLevel - kPocketDepth - kPocketFloor)),
            static_cast<int>(rng() % kChunkDepth)};
      if (!host(at)) continue;
    }
    unsigned kind = rng() % 10;
    BlockType gem = kind < 2   ? BlockType::DIAMOND_ORE
                    : kind < 6 ? BlockType::EMERALD_ORE
                               : BlockType::RUBY_ORE;
    // A small clump: grow from the spot through rock next to what's there.
    int want = kPocketMin + static_cast<int>(rng() % (kPocketMax - kPocketMin + 1));
    std::vector<std::pair<glm::ivec3, BlockType>> cells{{at, chunk.at(at.x, at.y, at.z)}};
    chunk.at(at.x, at.y, at.z) = gem;
    for (int tries = 0; static_cast<int>(cells.size()) < want && tries < 40; ++tries) {
      glm::ivec3 next = cells[rng() % cells.size()].first + kSides[rng() % 6];
      if (!host(next)) continue;
      cells.push_back({next, chunk.at(next.x, next.y, next.z)});
      chunk.at(next.x, next.y, next.z) = gem;
    }
    // Too cramped to make a pocket: leave the rock as it was.
    if (static_cast<int>(cells.size()) < kPocketMin)
      for (const auto &[p, was] : cells) chunk.at(p.x, p.y, p.z) = was;
  }
}

} // namespace caves
