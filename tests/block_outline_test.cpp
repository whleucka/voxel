#include "doctest/doctest.h"
#include "render/block_outline.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <glm/gtc/epsilon.hpp>

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
  CHECK(BlockOutline::target(world, eye, {0, 0, -1}, kPlayerReach) == glm::ivec3(8, kBase, 4));
  // Water in between is looked through.
  fill(world, {8, kBase, 6}, {8, kBase, 7}, BlockType::WATER);
  CHECK(BlockOutline::target(world, eye, {0, 0, -1}, kPlayerReach) == glm::ivec3(8, kBase, 4));
  // Only water, or only air, frames nothing.
  fill(world, {8, kBase, 9}, {8, kBase, 12}, BlockType::WATER);
  CHECK_FALSE(BlockOutline::target(world, eye, {0, 0, 1}, kPlayerReach));
  fill(world, {8, kBase, 4}, {8, kBase, 4}, BlockType::AIR);
  CHECK_FALSE(BlockOutline::target(world, eye, {0, 0, -1}, kPlayerReach));
  // Out of reach: nothing.
  fill(world, {8, kBase, 1}, {8, kBase, 1}, BlockType::STONE);
  CHECK(8.5f - 2.0f > kPlayerReach);
  CHECK_FALSE(BlockOutline::target(world, eye, {0, 0, -1}, kPlayerReach));
}

// ─── Shapes ────────────────────────────────────────────────────────────

namespace {

struct Cell {
  World world;
  static constexpr glm::ivec3 kAt{8, kBase, 8};
  Cell() {
    world.loadChunkSync(0, 0);
    fill(world, {0, kBase - 3, 0}, {15, kBase + 10, 15}, BlockType::AIR);
  }
  std::vector<DoorBox> boxes(BlockType b, glm::ivec3 at = kAt) {
    fill(world, at, at, b);
    return BlockOutline::boxes(world, at);
  }
};

bool is(const DoorBox &b, glm::vec3 lo, glm::vec3 hi) {
  return glm::all(glm::epsilonEqual(b.lo, lo, 1e-5f)) &&
         glm::all(glm::epsilonEqual(b.hi, hi, 1e-5f));
}

} // namespace

TEST_CASE("a full block is framed whole, a slab only its half") {
  Cell c;
  auto stone = c.boxes(BlockType::STONE);
  REQUIRE(stone.size() == 1);
  CHECK(is(stone[0], {0, 0, 0}, {1, 1, 1}));
  auto slab = c.boxes(BlockType::OAK_SLAB);
  REQUIRE(slab.size() == 1);
  CHECK(is(slab[0], {0, 0, 0}, {1, 0.5f, 1}));
}

TEST_CASE("a stair is framed as its two steps") {
  Cell c;
  auto stair = c.boxes(BlockType::OAK_STAIRS_N);
  CHECK(stair.size() == 2);
  CHECK(BlockOutline::mesh(stair).size() / 6 == 2 * 12 * 36);
}

TEST_CASE("a door is framed as its thin panel, over both halves") {
  Cell c;
  fill(c.world, Cell::kAt + glm::ivec3(0, 1, 0), Cell::kAt + glm::ivec3(0, 1, 0),
       BlockType::DOOR_TOP_N_CLOSED);
  auto bottom = c.boxes(BlockType::DOOR_BOTTOM_N_CLOSED);
  REQUIRE(bottom.size() == 1);
  CHECK(bottom[0].hi.y == 2.0f);
  CHECK(bottom[0].hi.z - bottom[0].lo.z == doctest::Approx(kDoorThickness));
  auto top = BlockOutline::boxes(c.world, Cell::kAt + glm::ivec3(0, 1, 0));
  REQUIRE(top.size() == 1);
  CHECK(top[0].lo.y == -1.0f);
}

TEST_CASE("a bed is framed low, round both its ends") {
  Cell c;
  const BlockType foot = bedBlock(BedColour::Red, false, DoorFacing::North);
  const BlockType head = bedBlock(BedColour::Red, true, DoorFacing::North);
  fill(c.world, bedOther(Cell::kAt, foot), bedOther(Cell::kAt, foot), head);
  auto bed = c.boxes(foot);
  REQUIRE(bed.size() == 1);
  CHECK(bed[0].hi.y == doctest::Approx(kBedHeight));
  const glm::vec3 size = bed[0].hi - bed[0].lo;
  CHECK(std::max(size.x, size.z) == 2.0f);
  CHECK(std::min(size.x, size.z) == 1.0f);
}

TEST_CASE("a fence is framed round its post and rails, a block tall; a ladder flat on its wall") {
  Cell c;
  auto post = c.boxes(BlockType::OAK_FENCE);
  REQUIRE(post.size() == 1);
  CHECK(post[0].hi.y == 1.0f);
  CHECK(post[0].hi.x - post[0].lo.x < 0.5f);
  fill(c.world, Cell::kAt + glm::ivec3(1, 0, 0), Cell::kAt + glm::ivec3(1, 0, 0),
       BlockType::OAK_FENCE);
  auto railed = BlockOutline::boxes(c.world, Cell::kAt);
  CHECK(railed[0].hi.x == 1.0f); // out to the east neighbour

  auto ladder = c.boxes(BlockType::LADDER_E);
  REQUIRE(ladder.size() == 1);
  CHECK(ladder[0].lo.x > 0.9f);
  CHECK(ladder[0].hi.y == 1.0f);
  auto torch = c.boxes(BlockType::TORCH);
  REQUIRE(torch.size() == 1);
  CHECK(torch[0].hi.x - torch[0].lo.x < 0.5f);
  CHECK(torch[0].hi.y < 1.0f);
}

TEST_CASE("something in front shortens the aim: the block behind isn't framed") {
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {0, kBase - 3, 0}, {15, kBase + 10, 15}, BlockType::AIR);
  fill(world, {8, kBase, 6}, {8, kBase, 6}, BlockType::STONE);
  const glm::vec3 eye(8.5f, kBase + 0.5f, 8.5f);
  CHECK(BlockOutline::target(world, eye, {0, 0, -1}, kPlayerReach));
  CHECK_FALSE(BlockOutline::target(world, eye, {0, 0, -1}, 1.0f)); // a boat 1 block away
}
