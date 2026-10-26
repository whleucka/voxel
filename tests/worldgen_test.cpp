// World generation must be a pure function of (seed, chunk position): the same
// chunk has to come out identical on every machine and every reload, whatever
// order or thread it was generated on.
#include "doctest/doctest.h"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
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
