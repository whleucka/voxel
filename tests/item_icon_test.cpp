#include "doctest/doctest.h"
#include "ui/item_icon_geometry.hpp"
#include "block/block_data.hpp"

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
  CHECK(coal.uv1 == glm::vec2(1.0f / 16));

  UvRect ruby = iconUv(itemDef(ItemId::RUBY).icon); // tile (3, 0)
  CHECK(ruby.uv0.x == doctest::Approx(3.0f / 16));

  UvRect dirt = iconUv(itemDef(itemForBlock(BlockType::DIRT)).icon); // tile (2, 0)
  CHECK(dirt.uv0.x == doctest::Approx(2.0f / 16));
  CHECK(dirt.uv1.x == doctest::Approx(3.0f / 16));
}

// A full block's faces: left (+Z), right (+X), top.
struct Cube {
  Quad left, right, top;
};

Cube cube(const Rect &slot, float padding) {
  IconFaces f = iconFaces(slot, padding, iconModel(BlockType::DIRT));
  REQUIRE(f.count == 3);
  return {f.face[0].quad, f.face[1].quad, f.face[2].quad};
}

TEST_CASE("cube faces stay inside the slot") {
  Rect slot{100, 200, 64, 64};
  Cube c = cube(slot, 6);
  for (const Quad *q : {&c.top, &c.left, &c.right})
    for (glm::vec2 p : q->p) {
      CHECK(p.x >= slot.x - 1e-3f);
      CHECK(p.x <= slot.right() + 1e-3f);
      CHECK(p.y >= slot.y - 1e-3f);
      CHECK(p.y <= slot.bottom() + 1e-3f);
    }
}

TEST_CASE("the top face sits above the sides, left face on the left") {
  Cube c = cube({0, 0, 64, 64}, 4);
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
  Cube c = cube(slot, 5);
  CHECK(c.top.p[0].x == doctest::Approx(slot.center().x)); // top point
  CHECK(c.top.p[0].y == doctest::Approx(slot.y + 5));
  CHECK(c.left.p[3].y == doctest::Approx(slot.bottom() - 5 - 10)); // left-bottom
  CHECK(c.right.p[2].x == doctest::Approx(slot.right() - 5));
}

TEST_CASE("a full block wears its whole top and side tiles") {
  const BlockTexture &grass = block_data.at(BlockType::GRASS);
  IconFaces f = iconFaces({0, 0, 64, 64}, 4, iconModel(BlockType::GRASS));
  UvRect side = tileUv(grass.side, kBlockAtlasTiles), top = tileUv(grass.top, kBlockAtlasTiles);
  CHECK(f.face[0].uv[0] == side.uv0);
  CHECK(f.face[0].uv[2] == side.uv1);
  CHECK(f.face[2].uv[0] == top.uv0);
  CHECK(f.face[2].uv[2] == top.uv1);
}

TEST_CASE("shaped blocks get their own icon models") {
  CHECK(iconModel(BlockType::COBBLESTONE).count == 1);
  CHECK(iconModel(BlockType::OAK_SLAB).count == 1);
  CHECK(iconModel(BlockType::OAK_SLAB).box[0].hi.y == doctest::Approx(kSlabHeight));
  CHECK(iconModel(BlockType::OAK_STAIRS_N).count == 2);
  CHECK(iconModel(BlockType::OAK_FENCE).count == 4);
  CHECK(iconModel(BlockType::FENCE_GATE_X_CLOSED).count == 5);
  CHECK(iconModel(BlockType::CAKE_4).box[0].hi.y == doctest::Approx(kCakeHeight));
}

TEST_CASE("icon boxes carry the block's bottom tile, for dropped items") {
  const BlockTexture &grass = block_data.at(BlockType::GRASS);
  CHECK(iconModel(BlockType::GRASS).box[0].bottom == grass.bottom);
  const BlockTexture &cake = block_data.at(BlockType::CAKE_4);
  CHECK(iconModel(BlockType::CAKE_4).box[0].bottom == cake.side); // not the cut
}

TEST_CASE("a slab's side shows the bottom half of its tile") {
  const BlockTexture &tex = block_data.at(BlockType::OAK_SLAB);
  IconFaces f = iconFaces({0, 0, 64, 64}, 4, iconModel(BlockType::OAK_SLAB));
  UvRect side = tileUv(tex.side, kBlockAtlasTiles);
  float mid = (side.uv0.y + side.uv1.y) * 0.5f;
  CHECK(f.face[0].uv[0].y == doctest::Approx(mid)); // top edge of the left face
  CHECK(f.face[0].uv[2].y == doctest::Approx(side.uv1.y));
}

TEST_CASE("a stair's step is painted over its base") {
  IconFaces f = iconFaces({0, 0, 64, 64}, 4, iconModel(BlockType::OAK_STAIRS_N));
  REQUIRE(f.count == 6);
  // The base comes first: its top face is at half height, below the step's.
  CHECK(centroid(f.face[2].quad).y > centroid(f.face[5].quad).y);
}

TEST_CASE("a fence's rails are painted before the post nearer the eye") {
  IconModel m = iconModel(BlockType::OAK_FENCE);
  IconFaces f = iconFaces({0, 0, 64, 64}, 4, m);
  // The far post (low X) first and the near one (high X) last.
  CHECK(centroid(f.face[0].quad).x < centroid(f.face[f.count - 3].quad).x);
  float near_x = centroid(f.face[f.count - 1].quad).x;
  for (int i = 0; i < f.count - 3; ++i) CHECK(centroid(f.face[i].quad).x < near_x + 1e-3f);
}
