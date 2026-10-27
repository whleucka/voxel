#include "doctest/doctest.h"
#include "render/block_outline.hpp"
#include "test_util.hpp"
#include "world/world.hpp"

namespace {
constexpr int kBase = 200; // clear sky above chunk (0,0)
}

TEST_CASE("the outline is 12 bars hugging the block's edges") {
  std::vector<float> v = BlockOutline::mesh();
  REQUIRE(v.size() % 6 == 0);
  CHECK(v.size() / 6 == 12 * 36); // 12 boxes, 36 vertices each
  const float t = BlockOutline::kThickness / 2 + BlockOutline::kInflate;
  for (size_t i = 0; i < v.size(); i += 6) {
    glm::vec3 p(v[i], v[i + 1], v[i + 2]);
    // Every corner is near the cube's surface and never far from an edge:
    // at least two coordinates sit at 0 or 1.
    int on_edge = 0;
    for (int a = 0; a < 3; ++a) {
      CHECK(p[a] >= -t - 1e-5f);
      CHECK(p[a] <= 1.0f + t + 1e-5f);
      on_edge += std::abs(p[a]) <= t + 1e-5f || std::abs(p[a] - 1.0f) <= t + 1e-5f;
    }
    CHECK(on_edge >= 2);
    CHECK(v[i + 3] < 0.2f); // dark
  }
}

TEST_CASE("the outline targets the aimed block in reach, through water, never air") {
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {0, kBase - 3, 0}, {15, kBase + 10, 15}, BlockType::AIR);
  fill(world, {8, kBase, 4}, {8, kBase, 4}, BlockType::STONE);
  const glm::vec3 eye(8.5f, kBase + 0.5f, 8.5f);

  // Straight ahead (-Z), 3.5 blocks away: framed.
  CHECK(BlockOutline::target(world, eye, {0, 0, -1}) == glm::ivec3(8, kBase, 4));
  // Water in between is looked through.
  fill(world, {8, kBase, 6}, {8, kBase, 7}, BlockType::WATER);
  CHECK(BlockOutline::target(world, eye, {0, 0, -1}) == glm::ivec3(8, kBase, 4));
  // Only water, or only air, frames nothing.
  fill(world, {8, kBase, 9}, {8, kBase, 12}, BlockType::WATER);
  CHECK_FALSE(BlockOutline::target(world, eye, {0, 0, 1}));
  fill(world, {8, kBase, 4}, {8, kBase, 4}, BlockType::AIR);
  CHECK_FALSE(BlockOutline::target(world, eye, {0, 0, -1}));
  // Out of reach: nothing.
  fill(world, {8, kBase, 1}, {8, kBase, 1}, BlockType::STONE);
  CHECK(8.5f - 2.0f > kPlayerReach);
  CHECK_FALSE(BlockOutline::target(world, eye, {0, 0, -1}));
}
