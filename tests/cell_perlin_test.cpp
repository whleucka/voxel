#include "doctest/doctest.h"
#include "chunk/chunk.hpp"
#include "util/cell_perlin.hpp"
#include <cstring>
#include <random>
#include <set>

TEST_CASE("CellPerlin gives glm::perlin's bits, down columns and at random") {
  std::mt19937 rng(9);
  std::uniform_real_distribution<float> offset(-100000.0f, 100000.0f);
  std::uniform_real_distribution<float> anywhere(-5000.0f, 5000.0f);
  CellPerlin perlin;
  auto same = [](float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; };
  int differ = 0;
  // As the minerals sample it: one column at a time, y innermost.
  for (int col = 0; col < 2000; ++col) {
    float ox = offset(rng), oz = offset(rng);
    int cx = static_cast<int>(rng() % 20000) - 10000, cz = static_cast<int>(rng() % 20000) - 10000;
    for (int y = 6; y < kChunkHeight - 5; ++y) {
      glm::vec3 p((cx * kChunkWidth + 3 + ox) * 0.05f, y * 0.05f, (cz * kChunkDepth + 7 + oz) * 0.05f);
      if (!same(glm::perlin(p), perlin(p))) ++differ;
    }
  }
  // A new cell every call.
  for (int i = 0; i < 100000; ++i) {
    glm::vec3 p(anywhere(rng), anywhere(rng), anywhere(rng));
    if (!same(glm::perlin(p), perlin(p))) ++differ;
  }
  CHECK(differ == 0);
}

TEST_CASE("chunk keys in view hash apart") {
  std::set<size_t> hashes;
  int keys = 0;
  for (int x = -20; x <= 20; ++x)
    for (int z = -20; z <= 20; ++z, ++keys) hashes.insert(ChunkKeyHash{}({x, z}));
  CHECK(hashes.size() == static_cast<size_t>(keys));
}
