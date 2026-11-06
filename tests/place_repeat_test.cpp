#include "doctest/doctest.h"
#include "player/place_repeat.hpp"

namespace {
RaycastResult hitFace(glm::ivec3 pos, glm::ivec3 normal, BlockType b = BlockType::STONE) {
  RaycastResult r;
  r.hit = true;
  r.block_pos = pos;
  r.normal = normal;
  r.block_type = b;
  return r;
}
} // namespace

TEST_CASE("placementFor: on the face hit, or over tall grass") {
  PlaceBlock p = placementFor(hitFace({0, 4, 0}, {0, 1, 0}));
  CHECK(p.pos == glm::ivec3(0, 5, 0));
  CHECK(p.against == glm::ivec3(0, -1, 0));
  PlaceBlock g = placementFor(hitFace({2, 5, 0}, {1, 0, 0}, BlockType::TALL_GRASS));
  CHECK(g.pos == glm::ivec3(2, 5, 0));
  CHECK(g.against == glm::ivec3(0, -1, 0));
}

TEST_CASE("a hold places one block per interval while the aim moves along") {
  PlaceRepeat hold(PlaceBlock{{0, 5, 0}, {0, -1, 0}}); // first block on the ground at y 5
  // Not due yet.
  CHECK_FALSE(hold.update(0.1f, hitFace({1, 4, 0}, {0, 1, 0})));
  auto next = hold.update(0.1f, hitFace({1, 4, 0}, {0, 1, 0}));
  REQUIRE(next);
  CHECK(next->pos == glm::ivec3(1, 5, 0));
  // The clock restarts after each one.
  CHECK_FALSE(hold.update(0.05f, hitFace({2, 4, 0}, {0, 1, 0})));
  CHECK(hold.update(0.15f, hitFace({2, 4, 0}, {0, 1, 0})));
}

TEST_CASE("a hold stays in the first block's layer: a floor doesn't become a pillar") {
  PlaceRepeat hold(PlaceBlock{{0, 5, 0}, {0, -1, 0}});
  // Aimed at the top of the block just placed: that would be y 6. Refused.
  CHECK_FALSE(hold.update(1.0f, hitFace({0, 5, 0}, {0, 1, 0})));
  // Its side is in the layer: the floor grows sideways.
  auto side = hold.update(0.0f, hitFace({0, 5, 0}, {0, 0, 1}));
  REQUIRE(side);
  CHECK(side->pos == glm::ivec3(0, 5, 1));
  CHECK(side->against == glm::ivec3(0, 0, -1));
}

TEST_CASE("a wall hold keeps to the wall's plane") {
  PlaceRepeat hold(PlaceBlock{{3, 5, 0}, {-1, 0, 0}}); // against a block's +x face
  CHECK_FALSE(hold.update(1.0f, hitFace({3, 5, 0}, {1, 0, 0}))); // would stick out to x 4
  auto up = hold.update(0.0f, hitFace({3, 5, 0}, {0, 1, 0}));
  REQUIRE(up);
  CHECK(up->pos == glm::ivec3(3, 6, 0));
}

TEST_CASE("a due block waits for a good aim, and nothing repeats the last spot") {
  PlaceRepeat hold(PlaceBlock{{0, 5, 0}, {0, -1, 0}});
  CHECK_FALSE(hold.update(1.0f, std::nullopt));                       // aimed at nothing
  CHECK_FALSE(hold.update(0.0f, hitFace({0, 4, 0}, {0, 1, 0})));      // the first spot again
  CHECK(hold.update(0.0f, hitFace({1, 4, 0}, {0, 1, 0})));            // at once, no wait
}

TEST_CASE("usedOnClick: doors and chests are used, plain blocks are built on") {
  CHECK(usedOnClick(BlockType::CHEST));
  CHECK(usedOnClick(BlockType::CRAFTING_TABLE));
  CHECK_FALSE(usedOnClick(BlockType::STONE));
  CHECK_FALSE(usedOnClick(BlockType::OAK_PLANKS));
}
