// Characterization tests for World block edits. These pin down today's
// behaviour so the sim-core refactor can be checked against it.
//
// Tests never call World::init() or World::update(): those load shaders and
// upload meshes, which needs a GL context. Mesh building itself is CPU-only.
#include "doctest/doctest.h"
#include "core/constants.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <chrono>
#include <thread>

namespace {

// Highest non-air block in a column, or -1.
int topSolidY(const World &world, int bx, int bz) {
  for (int y = kChunkHeight - 1; y >= 0; --y) {
    BlockType b = world.getBlockAt(glm::vec3(bx + 0.5f, y + 0.5f, bz + 0.5f));
    if (b != BlockType::AIR && b != BlockType::WATER) return y;
  }
  return -1;
}

uint32_t linearIndex(int lx, int y, int lz) {
  return lx + kChunkWidth * (lz + kChunkDepth * y);
}

} // namespace

TEST_CASE("placing a block is readable and recorded as an edit") {
  World world;
  world.loadChunkSync(0, 0);

  const glm::ivec3 pos{3, kChunkHeight - 2, 5};
  REQUIRE(world.getBlockAt(glm::vec3(pos) + 0.5f) == BlockType::AIR);

  world.setBlockAt(pos, BlockType::STONE);

  CHECK(world.getBlockAt(glm::vec3(pos) + 0.5f) == BlockType::STONE);
  WorldEdits edits = world.snapshotEdits();
  REQUIRE(edits.count({0, 0}) == 1);
  CHECK(edits[{0, 0}].at(linearIndex(3, pos.y, 5)) ==
        static_cast<uint8_t>(BlockType::STONE));
}

TEST_CASE("breaking a block records AIR") {
  World world;
  world.loadChunkSync(0, 0);

  int y = topSolidY(world, 7, 7);
  REQUIRE(y > 0);
  world.setBlockAt({7, y, 7}, BlockType::AIR);

  CHECK(world.getBlockAt(glm::vec3(7.5f, y + 0.5f, 7.5f)) == BlockType::AIR);
  WorldEdits edits = world.snapshotEdits();
  CHECK(edits[{0, 0}].at(linearIndex(7, y, 7)) ==
        static_cast<uint8_t>(BlockType::AIR));
}

TEST_CASE("edits outside the height range or loaded chunks are ignored") {
  World world;
  world.loadChunkSync(0, 0);

  world.setBlockAt({1, -1, 1}, BlockType::STONE);
  world.setBlockAt({1, kChunkHeight, 1}, BlockType::STONE);
  world.setBlockAt({100 * kChunkWidth, 10, 0}, BlockType::STONE); // unloaded

  CHECK(world.snapshotEdits().empty());
}

TEST_CASE("saved edits are replayed onto freshly loaded chunks") {
  WorldEdits saved;
  saved[{0, 0}][linearIndex(2, kChunkHeight - 2, 2)] =
      static_cast<uint8_t>(BlockType::SAND);

  World world;
  world.setLoadedEdits(saved);
  world.loadChunkSync(0, 0);

  CHECK(world.getBlockAt(glm::vec3(2.5f, kChunkHeight - 1.5f, 2.5f)) ==
        BlockType::SAND);
}

TEST_CASE("placing a light emitter lights its neighbourhood") {
  World world;
  world.loadChunkSync(0, 0);

  const glm::ivec3 pos{8, kChunkHeight - 4, 8};
  REQUIRE(world.getBlockLightAt(pos + glm::ivec3(1, 0, 0)) == 0);

  world.setBlockAt(pos, BlockType::GLOWSTONE);
  CHECK(world.getBlockLightAt(pos + glm::ivec3(1, 0, 0)) > 0);

  world.setBlockAt(pos, BlockType::AIR);
  CHECK(world.getBlockLightAt(pos + glm::ivec3(1, 0, 0)) == 0);
}

TEST_CASE("chunks stream around every interest point") {
  ScopedRenderDistance rd(2);
  World world;

  const glm::vec3 points[] = {
      {0.5f, 80.0f, 0.5f},
      {100.5f * kChunkWidth, 80.0f, 0.5f},
  };
  world.setInterestPoints(points);

  CHECK(waitForChunk(world, 0, 0));
  CHECK(waitForChunk(world, 1, 1));
  CHECK(waitForChunk(world, 100, 0));
  CHECK(waitForChunk(world, 99, -1));
  // Nothing in between is loaded.
  CHECK(world.getChunk(50, 0) == nullptr);
}

TEST_CASE("chunks far from every interest point are unloaded") {
  ScopedRenderDistance rd(2);
  World world;

  const glm::vec3 here[] = {{0.5f, 80.0f, 0.5f}};
  world.setInterestPoints(here);
  REQUIRE(waitForChunk(world, 0, 0));

  const glm::vec3 there[] = {{100.5f * kChunkWidth, 80.0f, 0.5f}};
  world.setInterestPoints(there);
  REQUIRE(waitForChunk(world, 100, 0));

  // Generation of the old area may still be in flight; poll until it drains
  // and the stale chunk has been dropped by a later streaming update.
  for (int i = 0; i < 400 && world.getChunk(0, 0); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const glm::vec3 nudge[] = {{101.5f * kChunkWidth, 80.0f, 0.5f}};
    const glm::vec3 back[] = {{100.5f * kChunkWidth, 80.0f, 0.5f}};
    world.setInterestPoints(i % 2 ? back : nudge);
  }
  CHECK(world.getChunk(0, 0) == nullptr);
}

TEST_CASE("findSpawn picks a standable column") {
  World world;
  glm::vec3 spawn = world.findSpawn();

  int cx = static_cast<int>(std::floor(spawn.x / kChunkWidth));
  int cz = static_cast<int>(std::floor(spawn.z / kChunkDepth));
  world.loadChunkSync(cx, cz);

  CHECK(world.isSolidBlock(static_cast<int>(std::floor(spawn.x)),
                           static_cast<int>(spawn.y) - 1,
                           static_cast<int>(std::floor(spawn.z))));
  CHECK(world.getBlockAt(spawn + glm::vec3(0, 0.5f, 0)) == BlockType::AIR);
  CHECK(world.getBlockAt(spawn + glm::vec3(0, 1.5f, 0)) == BlockType::AIR);
}
