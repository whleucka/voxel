// World::raycast picks the block under the crosshair, and its face normal is
// where a placed block goes.
#include "doctest/doctest.h"
#include "test_util.hpp"
#include "world/world.hpp"
#include <cmath>

namespace {

constexpr int kSky = 200; // clear air in every generated chunk

// Chunk (0,0) loaded, with a stone block at `target` and the ray starting
// from the middle of the cell `from`.
struct RayRig {
  World world;
  glm::ivec3 target{8, kSky, 8};
  RayRig() {
    world.loadChunkSync(0, 0);
    fill(world, target, target, BlockType::STONE);
  }
  RaycastResult cast(glm::vec3 from, glm::vec3 dir, float reach = 8.0f, bool liquids = false) {
    return world.raycast(from, dir, reach, liquids);
  }
};

glm::vec3 centre(glm::ivec3 c) { return glm::vec3(c) + 0.5f; }

} // namespace

TEST_CASE_FIXTURE(RayRig, "a ray along each axis hits the block and reports the face it entered") {
  const glm::ivec3 dirs[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  for (glm::ivec3 d : dirs) {
    CAPTURE(d);
    glm::ivec3 from = target - d * 3; // three cells back along the ray
    RaycastResult r = cast(centre(from), glm::vec3(d));
    REQUIRE(r.hit);
    CHECK(r.block_pos == target);
    CHECK(r.block_type == BlockType::STONE);
    CHECK(r.normal == -d);
    CHECK(r.distance == doctest::Approx(2.5f));
  }
}

TEST_CASE_FIXTURE(RayRig, "a diagonal ray hits the block and names a face it can enter") {
  glm::vec3 from = centre(target) + glm::vec3(-2.0f, 2.2f, -2.4f);
  RaycastResult r = cast(from, centre(target) - from);
  REQUIRE(r.hit);
  CHECK(r.block_pos == target);
  // Whatever face it names, the cell in front of it is air the ray came through.
  CHECK(std::abs(r.normal.x) + std::abs(r.normal.y) + std::abs(r.normal.z) == 1);
  CHECK(glm::dot(glm::vec3(r.normal), from - centre(target)) > 0.0f);
}

TEST_CASE_FIXTURE(RayRig, "a block past the reach is missed") {
  glm::ivec3 from = target - glm::ivec3(6, 0, 0);
  CHECK_FALSE(cast(centre(from), {1, 0, 0}, 5.0f).hit);
  CHECK(cast(centre(from), {1, 0, 0}, 6.0f).hit);
}

TEST_CASE_FIXTURE(RayRig, "rays work across chunk borders and at negative coordinates") {
  world.loadChunkSync(-1, -1);
  glm::ivec3 far{-3, kSky, -5};
  fill(world, far, far, BlockType::STONE);
  RaycastResult r = cast(centre({2, kSky, -5}), {-1, 0, 0});
  REQUIRE(r.hit);
  CHECK(r.block_pos == far);
  CHECK(r.normal == glm::ivec3(1, 0, 0));
}

TEST_CASE_FIXTURE(RayRig, "water is passed through, unless the ray is looking for liquids") {
  glm::ivec3 water = target - glm::ivec3(2, 0, 0);
  fill(world, water, water, BlockType::WATER);
  glm::vec3 from = centre(target - glm::ivec3(4, 0, 0));
  CHECK(cast(from, {1, 0, 0}).block_pos == target);
  RaycastResult wet = cast(from, {1, 0, 0}, 8.0f, true);
  REQUIRE(wet.hit);
  CHECK(wet.block_pos == water);
}

TEST_CASE_FIXTURE(RayRig, "a zero direction hits nothing") {
  CHECK_FALSE(cast(centre(target) - glm::vec3(2, 0, 0), {0, 0, 0}).hit);
}
