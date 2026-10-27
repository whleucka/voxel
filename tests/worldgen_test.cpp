// World generation must be a pure function of (seed, chunk position): the same
// chunk has to come out identical on every machine and every reload, whatever
// order or thread it was generated on.
#include "doctest/doctest.h"
#include "biome/biome.hpp"
#include "biome/caves.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "core/seed.hpp"
#include "core/settings.hpp"
#include <algorithm>
#include <thread>
#include <vector>

namespace {

std::vector<BlockType> blocksOf(const Chunk &c) {
  std::vector<BlockType> out;
  out.reserve(kChunkWidth * kChunkHeight * kChunkDepth);
  for (int y = 0; y < kChunkHeight; ++y)
    for (int z = 0; z < kChunkDepth; ++z)
      for (int x = 0; x < kChunkWidth; ++x) out.push_back(c.at(x, y, z));
  return out;
}

std::vector<BlockType> generate(int cx, int cz) {
  Chunk c(cx, cz);
  c.init();
  return blocksOf(c);
}

} // namespace

TEST_CASE("a chunk generates identically regardless of what ran before it") {
  // Covers several biomes, so trees, ores and the bedrock layer are exercised.
  for (int cx = -2; cx <= 2; ++cx) {
    for (int cz = -2; cz <= 2; ++cz) {
      auto first = generate(cx, cz);
      generate(cx + 7, cz - 3); // perturb any shared generator state
      auto second = generate(cx, cz);
      CHECK_MESSAGE(first == second, "chunk ", cx, ",", cz);
    }
  }
}

TEST_CASE("a chunk generates identically on another thread") {
  auto here = generate(1, -1);

  std::vector<BlockType> there;
  std::thread t([&] {
    generate(40, 40);
    there = generate(1, -1);
  });
  t.join();

  CHECK(here == there);
}

TEST_CASE("different seeds give different chunks") {
  uint32_t saved = g_settings.world_seed;
  g_settings.world_seed = 1;
  auto a = generate(0, 0);
  g_settings.world_seed = 2;
  auto b = generate(0, 0);
  g_settings.world_seed = saved;

  // Terrain shape follows noise_offset (unchanged here); the seeded parts
  // (bedrock mix, ores, tree shapes) must still differ.
  CHECK(a != b);
}

// ─── Old worlds keep their terrain ─────────────────────────────────────

namespace {

uint64_t fnv1a(const std::vector<BlockType> &blocks) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (BlockType b : blocks) {
    h ^= static_cast<uint8_t>(b);
    h *= 0x100000001b3ull;
  }
  return h;
}

// Seeds the generator like a loaded world, and puts it back afterwards.
struct SeededWorld {
  uint32_t saved_seed = g_settings.world_seed;
  glm::vec2 saved_offset = g_settings.noise_offset;
  uint8_t saved_worldgen = g_settings.worldgen;
  SeededWorld(uint32_t seed, uint8_t version) {
    g_settings.world_seed = seed;
    g_settings.noise_offset = noiseOffsetFor(seed);
    g_settings.worldgen = version;
  }
  ~SeededWorld() {
    g_settings.world_seed = saved_seed;
    g_settings.noise_offset = saved_offset;
    g_settings.worldgen = saved_worldgen;
  }
};

} // namespace

TEST_CASE("version 1 terrain is block-for-block what it was before caves") {
  // Recorded on main at 8288eb2, before caves existed. A world saved
  // before caves must regenerate exactly, or edits land on the wrong terrain.
  SeededWorld world(12345, worldgen::kOriginal);
  const std::pair<glm::ivec2, uint64_t> golden[] = {
      {{0, 0}, 7253626337975457968ull},    {{5, -3}, 9952782697928643798ull},
      {{-20, 14}, 10457763427820496660ull}, {{37, 2}, 3171172932800387816ull},
      {{-8, -40}, 7604037320953062175ull},  {{90, 61}, 3652691629932540463ull},
  };
  for (const auto &[pos, hash] : golden) {
    CAPTURE(pos.x);
    CAPTURE(pos.y);
    CHECK(fnv1a(generate(pos.x, pos.y)) == hash);
  }
}

// ─── Caves (version 2) ─────────────────────────────────────────────────

namespace {

int surfaceOf(int wx, int wz) {
  return std::min(Biome::getHeight({wx, wz}), kChunkHeight - 1);
}

bool isLog(BlockType b) {
  return b == BlockType::OAK_LOG || b == BlockType::PINE_LOG || b == BlockType::PALM_LOG ||
         b == BlockType::CHERRY_LOG;
}

// Chunks spread over land, coast and sea for seed 12345.
const glm::ivec2 kSample[] = {{-40, -40}, {-40, 0}, {-20, 20}, {0, -20}, {0, 40},
                              {20, -40},  {20, 20}, {40, 0},   {40, 40}, {-60, 10}};

// Cave cells: air (or what decorate() puts in a cave) below the ground.
bool inCave(BlockType b) {
  return b == BlockType::AIR || b == BlockType::WATER || b == BlockType::GLOW_CRYSTAL;
}

} // namespace

TEST_CASE("version 2 carves caves; version 1 has none") {
  for (uint8_t version : {worldgen::kOriginal, worldgen::kCaves}) {
    SeededWorld world(12345, version);
    long carved = 0;
    for (glm::ivec2 pos : kSample) {
      Chunk c(pos.x, pos.y);
      c.init();
      for (int x = 0; x < kChunkWidth; ++x)
        for (int z = 0; z < kChunkDepth; ++z) {
          int h = surfaceOf(pos.x * kChunkWidth + x, pos.y * kChunkDepth + z);
          for (int y = 0; y < h; ++y) carved += c.at(x, y, z) == BlockType::AIR;
        }
    }
    CAPTURE(version);
    if (version == worldgen::kOriginal) CHECK(carved == 0);
    else CHECK(carved > 500);
  }
}

TEST_CASE("caves follow one world-wide pattern, so they meet across chunk borders") {
  SeededWorld world(12345, worldgen::kCaves);
  // Two neighbours: x = 15 of one and x = 0 of the next are side by side.
  for (glm::ivec2 pos : {glm::ivec2{-40, 0}, glm::ivec2{-39, 0}, glm::ivec2{20, 20}}) {
    Chunk c(pos.x, pos.y);
    c.init();
    for (int x : {0, 7, 15})
      for (int z : {0, 15}) {
        int wx = pos.x * kChunkWidth + x, wz = pos.y * kChunkDepth + z;
        int h = surfaceOf(wx, wz);
        for (int y = 1; y < h; ++y) {
          CAPTURE(wx);
          CAPTURE(y);
          CAPTURE(wz);
          if (caves::isTunnel({wx, y, wz})) CHECK(inCave(c.at(x, y, z)));
          else CHECK(c.at(x, y, z) != BlockType::AIR);
        }
      }
  }
}

TEST_CASE("caves keep away from the sea and only open onto high ground") {
  SeededWorld world(12345, worldgen::kCaves);
  long openings = 0;
  for (glm::ivec2 pos : kSample) {
    Chunk c(pos.x, pos.y);
    c.init();
    for (int x = 0; x < kChunkWidth; ++x)
      for (int z = 0; z < kChunkDepth; ++z) {
        int h = surfaceOf(pos.x * kChunkWidth + x, pos.y * kChunkDepth + z);
        if (h > 0 && c.at(x, h - 1, z) == BlockType::AIR) {
          ++openings;
          CHECK(h > kSeaLevel + caves::kOpenAbove);
        }
        for (int y = caves::kFloor; y < h; ++y) {
          if (c.at(x, y, z) != BlockType::AIR) continue;
          // No sea or lake water (water at or above its column's ground) next to a cave.
          const glm::ivec3 sides[] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}};
          for (glm::ivec3 d : sides) {
            int nx = x + d.x, ny = y + d.y, nz = z + d.z;
            if (nx < 0 || nx >= kChunkWidth || nz < 0 || nz >= kChunkDepth) continue;
            int nh = surfaceOf(pos.x * kChunkWidth + nx, pos.y * kChunkDepth + nz);
            if (ny >= nh) CHECK(c.at(nx, ny, nz) != BlockType::WATER);
          }
        }
      }
  }
  CHECK(openings > 0);
}

TEST_CASE("no tree floats over a cave mouth") {
  SeededWorld world(12345, worldgen::kCaves);
  for (glm::ivec2 pos : kSample) {
    Chunk c(pos.x, pos.y);
    c.init();
    for (int x = 0; x < kChunkWidth; ++x)
      for (int z = 0; z < kChunkDepth; ++z)
        for (int y = 1; y < kChunkHeight; ++y)
          if (isLog(c.at(x, y, z))) CHECK(c.at(x, y - 1, z) != BlockType::AIR);
  }
}
