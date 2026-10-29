#include "doctest/doctest.h"
#include "physics/path.hpp"
#include "block/door.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <algorithm>

namespace {

constexpr int kBase = 200; // clear sky above the generated terrain

struct PathRig {
  World world;
  explicit PathRig(int chunks_x = 1) {
    for (int cx = 0; cx < chunks_x; ++cx) world.loadChunkSync(cx, 0);
  }
  void floor(glm::ivec3 lo, glm::ivec3 hi, int y = kBase) {
    fill(world, {lo.x, y, lo.z}, {hi.x, y, hi.z}, BlockType::STONE);
  }
  std::optional<std::vector<glm::ivec3>> path(glm::ivec3 from, glm::ivec3 to,
                                              PathParams p = {}) {
    return findPath(world, from, to, p);
  }
};

const glm::ivec3 kUp{0, 1, 0};

// Each step moves one cell sideways, and at most one up or max_drop down.
void checkSteps(glm::ivec3 from, const std::vector<glm::ivec3> &path, int max_drop = 3) {
  glm::ivec3 prev = from;
  for (glm::ivec3 c : path) {
    CHECK(std::abs(c.x - prev.x) + std::abs(c.z - prev.z) == 1);
    CHECK(c.y - prev.y <= 1);
    CHECK(prev.y - c.y <= max_drop);
    prev = c;
  }
}

} // namespace

TEST_CASE("a straight path on flat ground is the shortest one") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  glm::ivec3 from{2, kBase + 1, 8}, to{12, kBase + 1, 8};
  auto p = r.path(from, to);
  REQUIRE(p);
  CHECK(p->size() == 10);
  CHECK(p->back() == to);
  checkSteps(from, *p);
}

TEST_CASE("already there is an empty path") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  auto p = r.path({5, kBase + 1, 5}, {5, kBase + 1, 5});
  REQUIRE(p);
  CHECK(p->empty());
}

TEST_CASE("a path goes around a wall through the gap") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  // A wall across x = 8, open only at z = 13.
  fill(r.world, {8, kBase + 1, 1}, {8, kBase + 3, 12}, BlockType::STONE);
  glm::ivec3 from{4, kBase + 1, 4}, to{12, kBase + 1, 4};
  auto p = r.path(from, to);
  REQUIRE(p);
  CHECK(p->back() == to);
  checkSteps(from, *p);
  CHECK(std::find(p->begin(), p->end(), glm::ivec3(8, kBase + 1, 13)) != p->end());
}

TEST_CASE("a path climbs a one-block step") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  r.floor({8, 0, 1}, {14, 0, 14}, kBase + 1); // the right half is a block higher
  glm::ivec3 from{2, kBase + 1, 8}, to{12, kBase + 2, 8};
  auto p = r.path(from, to);
  REQUIRE(p);
  CHECK(p->back() == to);
  checkSteps(from, *p);
}

TEST_CASE("a path won't climb two blocks") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  fill(r.world, {8, kBase + 1, 1}, {14, kBase + 2, 14}, BlockType::STONE);
  CHECK_FALSE(r.path({2, kBase + 1, 8}, {12, kBase + 3, 8}));
}

TEST_CASE("a path drops three blocks but not four") {
  for (int height : {3, 4}) {
    CAPTURE(height);
    PathRig r;
    r.floor({1, 0, 1}, {14, 0, 14});
    r.floor({1, 0, 1}, {6, 0, 14}, kBase + height); // a ledge, open underneath
    glm::ivec3 from{3, kBase + height + 1, 8}, to{12, kBase + 1, 8};
    auto p = r.path(from, to);
    CHECK(p.has_value() == (height <= 3));
    if (p) checkSteps(from, *p);
  }
}

TEST_CASE("a path needs headroom for the creature's height") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  // Walls along z = 7 and 9 with a low roof between them: a 1-high tunnel.
  fill(r.world, {1, kBase + 1, 7}, {14, kBase + 2, 7}, BlockType::STONE);
  fill(r.world, {1, kBase + 1, 9}, {14, kBase + 2, 9}, BlockType::STONE);
  fill(r.world, {1, kBase + 2, 8}, {14, kBase + 2, 8}, BlockType::STONE);
  fill(r.world, {1, kBase + 1, 1}, {14, kBase + 3, 6}, BlockType::STONE);
  fill(r.world, {1, kBase + 1, 10}, {14, kBase + 3, 14}, BlockType::STONE);
  glm::ivec3 from{2, kBase + 1, 8}, to{12, kBase + 1, 8};

  PathParams tall;
  CHECK_FALSE(r.path(from, to, tall));
  PathParams small;
  small.height_cells = 1;
  CHECK(r.path(from, to, small));
}

TEST_CASE("no path to an island") {
  PathRig r;
  r.floor({1, 0, 1}, {6, 0, 14});
  r.floor({10, 0, 1}, {14, 0, 14}, kBase - 10); // far below: too big a drop
  CHECK_FALSE(r.path({2, kBase + 1, 8}, {12, kBase - 9, 8}));
}

TEST_CASE("the node limit gives up on long searches") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  glm::ivec3 from{2, kBase + 1, 2}, to{13, kBase + 1, 13};
  PathParams p;
  p.max_nodes = 5;
  CHECK_FALSE(r.path(from, to, p));
  CHECK(r.path(from, to)); // within the default limit
}

TEST_CASE("targets beyond the range limit aren't searched") {
  PathRig r(3);
  r.floor({0, 0, 1}, {47, 0, 14});
  glm::ivec3 from{2, kBase + 1, 8};
  PathParams p;
  p.max_nodes = 10000;
  CHECK(r.path(from, {30, kBase + 1, 8}, p));
  CHECK_FALSE(r.path(from, {40, kBase + 1, 8}, p));
}

TEST_CASE("mid-air endpoints snap to the ground below") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  auto p = r.path({2, kBase + 2, 8}, {6, kBase + 3, 8}); // a jumping player
  REQUIRE(p);
  CHECK(p->back() == glm::ivec3(6, kBase + 1, 8));
}

TEST_CASE("creatures path through an open door but not a closed one") {
  for (bool open : {false, true}) {
    CAPTURE(open);
    PathRig r;
    r.floor({1, 0, 1}, {14, 0, 14});
    // A wall across x = 8 the whole way, with a door in it at z = 4.
    fill(r.world, {8, kBase + 1, 1}, {8, kBase + 3, 14}, BlockType::STONE);
    fill(r.world, {8, kBase + 1, 4}, {8, kBase + 1, 4}, doorBlock(false, DoorFacing::East, open));
    fill(r.world, {8, kBase + 2, 4}, {8, kBase + 2, 4}, doorBlock(true, DoorFacing::East, open));
    glm::ivec3 from{4, kBase + 1, 4}, to{12, kBase + 1, 4};
    auto p = r.path(from, to);
    if (!open) {
      CHECK_FALSE(p);
      continue;
    }
    REQUIRE(p);
    checkSteps(from, *p);
    CHECK(std::find(p->begin(), p->end(), glm::ivec3(8, kBase + 1, 4)) != p->end());
  }
}

TEST_CASE("creatures go round a bed, never over it") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  // A bed lying across the straight line from start to goal.
  fill(r.world, {8, kBase + 1, 8}, {8, kBase + 1, 8}, bedBlock(BedColour::Blue, false, DoorFacing::South));
  fill(r.world, {8, kBase + 1, 9}, {8, kBase + 1, 9}, bedBlock(BedColour::Blue, true, DoorFacing::South));
  glm::ivec3 from{4, kBase + 1, 8}, to{12, kBase + 1, 8};
  auto p = r.path(from, to);
  REQUIRE(p);
  checkSteps(from, *p);
  for (glm::ivec3 c : *p) {
    CAPTURE(c);
    CHECK(c.y == kBase + 1); // never up on top of it
    CHECK_FALSE(isBed(r.world.getBlockAt(glm::vec3(c) + 0.5f)));
  }

  // A bed plugging the only gap in a wall: no way through.
  PathRig w;
  w.floor({1, 0, 1}, {14, 0, 14});
  fill(w.world, {8, kBase + 1, 1}, {8, kBase + 3, 14}, BlockType::STONE);
  fill(w.world, {8, kBase + 1, 4}, {8, kBase + 3, 4}, BlockType::AIR);
  fill(w.world, {8, kBase + 1, 4}, {8, kBase + 1, 4}, bedBlock(BedColour::Blue, false, DoorFacing::East));
  CHECK_FALSE(w.path({4, kBase + 1, 4}, {12, kBase + 1, 4}));
}

TEST_CASE("a path walks straight through tall grass, flowers and torches") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  fill(r.world, {8, kBase + 1, 1}, {8, kBase + 1, 14}, BlockType::TALL_GRASS);
  fill(r.world, {9, kBase + 1, 1}, {9, kBase + 1, 14}, BlockType::POPPY);
  fill(r.world, {10, kBase + 1, 1}, {10, kBase + 1, 14}, BlockType::TORCH);
  glm::ivec3 from{4, kBase + 1, 8}, to{12, kBase + 1, 8};
  auto p = r.path(from, to);
  REQUIRE(p);
  CHECK(p->size() == 8);
}

TEST_CASE("a cactus is a wall to a path") {
  PathRig r;
  r.floor({1, 0, 1}, {14, 0, 14});
  fill(r.world, {8, kBase + 1, 1}, {8, kBase + 2, 12}, BlockType::CACTUS);
  glm::ivec3 from{4, kBase + 1, 4}, to{12, kBase + 1, 4};
  auto p = r.path(from, to);
  REQUIRE(p);
  CHECK(std::find(p->begin(), p->end(), glm::ivec3(8, kBase + 1, 13)) != p->end());
}
