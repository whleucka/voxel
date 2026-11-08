#include "doctest/doctest.h"
#include "player/place_repeat.hpp"
#include "core/constants.hpp"

namespace {
RaycastResult hitFace(glm::ivec3 pos, glm::ivec3 normal, BlockType b = BlockType::STONE,
                      float up = 0.25f) {
  RaycastResult r;
  r.hit = true;
  r.block_pos = pos;
  r.normal = normal;
  r.block_type = b;
  // On the face, `up` of the way up the block.
  r.point = glm::vec3(pos) + glm::vec3(0.5f) + glm::vec3(normal) * 0.5f;
  if (normal.y == 0) r.point.y = static_cast<float>(pos.y) + up;
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

TEST_CASE("placementFor: a side hit above its middle is the upper half") {
  CHECK(placementFor(hitFace({0, 4, 0}, {1, 0, 0}, BlockType::STONE, 0.75f)).upper);
  CHECK_FALSE(placementFor(hitFace({0, 4, 0}, {1, 0, 0}, BlockType::STONE, 0.25f)).upper);
  // A top or a bottom face has no half; the face says which.
  CHECK_FALSE(placementFor(hitFace({0, 4, 0}, {0, 1, 0})).upper);
  CHECK_FALSE(placementFor(hitFace({0, 4, 0}, {0, -1, 0})).upper);
}

TEST_CASE("a hold keeps to the half it began in") {
  PlaceRepeat hold(PlaceBlock{{0, 5, -1}, {0, 0, 1}, true});
  // Even aimed low on the next side, its slabs go in the top half.
  auto next = hold.update(1.0f, hitFace({0, 5, -1}, {0, 0, -1}, BlockType::STONE, 0.25f));
  REQUIRE(next);
  CHECK(next->upper);
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

TEST_CASE("a hold keeps to the face it began on: walking backward lays a bridge") {
  // Started on the -z side of the block at the edge (z 0): the first block at z -1.
  PlaceRepeat hold(PlaceBlock{{0, 5, -1}, {0, 0, 1}});
  // Backing up, aimed at the -z side of the newest block: the next one beyond it.
  auto next = hold.update(1.0f, hitFace({0, 5, -1}, {0, 0, -1}));
  REQUIRE(next);
  CHECK(next->pos == glm::ivec3(0, 5, -2));
  auto again = hold.update(1.0f, hitFace({0, 5, -2}, {0, 0, -1}));
  REQUIRE(again);
  CHECK(again->pos == glm::ivec3(0, 5, -3));
  // Its top, or a side facing another way, isn't where this hold builds.
  CHECK_FALSE(hold.update(1.0f, hitFace({0, 5, -3}, {0, 1, 0})));
  CHECK_FALSE(hold.update(1.0f, hitFace({0, 5, -3}, {1, 0, 0})));
}

TEST_CASE("a hold begun on a top builds on tops") {
  PlaceRepeat hold(PlaceBlock{{0, 5, 0}, {0, -1, 0}});
  CHECK_FALSE(hold.update(1.0f, hitFace({0, 5, 0}, {0, 0, 1}))); // a side: no
  auto top = hold.update(0.0f, hitFace({1, 4, 0}, {0, 1, 0}));
  REQUIRE(top);
  CHECK(top->pos == glm::ivec3(1, 5, 0));
  // Tall grass is built over as if on the top below it.
  auto grass = hold.update(1.0f, hitFace({2, 5, 0}, {1, 0, 0}, BlockType::TALL_GRASS));
  REQUIRE(grass);
  CHECK(grass->pos == glm::ivec3(2, 5, 0));
}

TEST_CASE("a due block waits for a good aim, and nothing repeats the last spot") {
  PlaceRepeat hold(PlaceBlock{{0, 5, 0}, {0, -1, 0}});
  CHECK_FALSE(hold.update(1.0f, std::nullopt));                       // aimed at nothing
  CHECK_FALSE(hold.update(0.0f, hitFace({0, 4, 0}, {0, 1, 0})));      // the first spot again
  CHECK(hold.update(0.0f, hitFace({1, 4, 0}, {0, 1, 0})));            // at once, no wait
}

TEST_CASE("a hold keeps up with a builder flying: a block about every block moved") {
  CHECK(PlaceRepeat::intervalAt(0.0f) == PlaceRepeat::kInterval);
  CHECK(PlaceRepeat::intervalAt(kPlayerMoveSpeed) == PlaceRepeat::kInterval);
  const float fly = kPlayerMoveSpeed * 3.0f;
  CHECK(PlaceRepeat::intervalAt(fly) * fly <= 1.0f); // no gap in the row
  CHECK(PlaceRepeat::intervalAt(1000.0f) == PlaceRepeat::kMinInterval);

  // Flying backward along a bridge at 13 blocks/s: one block every ~0.07 s.
  PlaceRepeat hold(PlaceBlock{{0, 5, -1}, {0, 0, 1}});
  const float dt = 0.07f;
  glm::vec3 eye(0.5f, 6.6f, 2.0f);
  CHECK_FALSE(hold.update(dt, std::nullopt, eye)); // the first frame has no speed yet
  int placed = 0;
  for (int z = -1; z > -6; --z) {
    eye.z -= fly * dt;
    if (hold.update(dt, hitFace({0, 5, z}, {0, 0, -1}), eye)) ++placed;
  }
  CHECK(placed == 5);
}

TEST_CASE("usedOnClick: doors and chests are used, plain blocks are built on") {
  CHECK(usedOnClick(BlockType::CHEST));
  CHECK(usedOnClick(BlockType::CRAFTING_TABLE));
  CHECK_FALSE(usedOnClick(BlockType::STONE));
  CHECK_FALSE(usedOnClick(BlockType::OAK_PLANKS));
}
