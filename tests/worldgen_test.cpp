// World generation must be a pure function of (seed, chunk position): the same
// chunk has to come out identical on every machine and every reload, whatever
// order or thread it was generated on.
#include "doctest/doctest.h"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "core/seed.hpp"
#include "core/settings.hpp"
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
  explicit SeededWorld(uint32_t seed) {
    g_settings.world_seed = seed;
    g_settings.noise_offset = noiseOffsetFor(seed);
  }
  ~SeededWorld() {
    g_settings.world_seed = saved_seed;
    g_settings.noise_offset = saved_offset;
  }
};

} // namespace

TEST_CASE("version 1 terrain is block-for-block what it was before caves") {
  // Recorded on main at 8288eb2, before caves existed. A world saved
  // before caves must regenerate exactly, or edits land on the wrong terrain.
  SeededWorld world(12345);
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
