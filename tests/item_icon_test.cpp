#include "doctest/doctest.h"
#include "ui/item_icon_geometry.hpp"

namespace {

glm::vec2 centroid(const Quad &q) {
  return (q.p[0] + q.p[1] + q.p[2] + q.p[3]) * 0.25f;
}

} // namespace

TEST_CASE("block tiles map into the 16-wide atlas") {
  UvRect uv = tileUv({0, 0}, kBlockAtlasTiles);
  CHECK(uv.uv0 == glm::vec2(0.0f));
  CHECK(uv.uv1 == glm::vec2(1.0f / 16));

  uv = tileUv({14, 1}, kBlockAtlasTiles);
  CHECK(uv.uv0.x == doctest::Approx(14.0f / 16));
  CHECK(uv.uv0.y == doctest::Approx(1.0f / 16));
  CHECK(uv.uv1.x == doctest::Approx(15.0f / 16));
  CHECK(uv.uv1.y == doctest::Approx(2.0f / 16));
}

TEST_CASE("icons pick the atlas their item lives in") {
  UvRect coal = iconUv(itemDef(ItemId::COAL).icon);
  CHECK(coal.uv0 == glm::vec2(0.0f));
  CHECK(coal.uv1 == glm::vec2(1.0f / 8));

  UvRect ruby = iconUv(itemDef(ItemId::RUBY).icon); // tile (3, 0)
  CHECK(ruby.uv0.x == doctest::Approx(3.0f / 8));

  UvRect dirt = iconUv(itemDef(itemForBlock(BlockType::DIRT)).icon); // tile (2, 0)
  CHECK(dirt.uv0.x == doctest::Approx(2.0f / 16));
  CHECK(dirt.uv1.x == doctest::Approx(3.0f / 16));
}

TEST_CASE("cube faces stay inside the slot") {
  Rect slot{100, 200, 64, 64};
  CubeQuads c = cubeQuads(slot, 6);
  for (const Quad *q : {&c.top, &c.left, &c.right})
    for (glm::vec2 p : q->p) {
      CHECK(p.x >= slot.x);
      CHECK(p.x <= slot.right());
      CHECK(p.y >= slot.y);
      CHECK(p.y <= slot.bottom());
    }
}

TEST_CASE("the top face sits above the sides, left face on the left") {
  CubeQuads c = cubeQuads({0, 0, 64, 64}, 4);
  CHECK(centroid(c.top).y < centroid(c.left).y);
  CHECK(centroid(c.top).y < centroid(c.right).y);
  CHECK(centroid(c.left).x < centroid(c.right).x);
  // The faces share the front edge.
  CHECK(c.left.p[1] == c.right.p[0]);
  CHECK(c.left.p[2] == c.right.p[3]);
  CHECK(c.top.p[2] == c.left.p[1]);
}

TEST_CASE("the cube is centred and uses the padded size") {
  Rect slot{10, 20, 50, 50};
  CubeQuads c = cubeQuads(slot, 5);
  CHECK(c.top.p[0].x == doctest::Approx(slot.center().x)); // top point
  CHECK(c.top.p[0].y == doctest::Approx(slot.y + 5));
  CHECK(c.left.p[3].y == doctest::Approx(slot.bottom() - 5 - 10)); // left-bottom
  CHECK(c.right.p[2].x == doctest::Approx(slot.right() - 5));
}
