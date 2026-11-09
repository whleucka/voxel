#include "doctest/doctest.h"
#include "player/place_repeat.hpp"
#include "core/constants.hpp"
#include <set>
#include <tuple>

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

namespace {
// Flies a builder backward off the end of a bridge, holding place, against a
// Game that puts sent blocks in only on its 20 Hz tick. Returns the nearest
// cell of an unbroken bridge (z 3 down to it), its far end being z 0.
// `pitch` is how far down the builder looks (degrees); `wobble` sways the aim
// up and down and side to side by that much, as a hand on a mouse does.
int flyBackBridging(float speed, float fps, float seconds, float pitch_deg = 70.0f,
                    float wobble_deg = 0.0f) {
  std::set<std::tuple<int, int, int>> cells;
  for (int z = 0; z <= 3; ++z) cells.insert({0, 5, z});
  auto solid = [&](glm::ivec3 c) { return cells.count({c.x, c.y, c.z}) > 0; };
  auto cast = [&](glm::vec3 o, glm::vec3 d) -> std::optional<RaycastResult> {
    std::optional<RaycastResult> best;
    for (auto [x, y, z] : cells)
      if (auto hit = rayIntoCell(o, d, kPlayerReach, {x, y, z});
          hit && (!best || hit->distance < best->distance))
        best = hit;
    return best;
  };
  // Feet on the bridge's top (y 6), just past its end, facing back along it
  // and looking down at its end's side.
  // Far enough past it that the first click hits the end's side at its middle.
  glm::vec3 eye(0.5f, 7.62f, -2.12f / std::tan(glm::radians(pitch_deg)));
  auto look = [&](float t) {
    const float pitch = glm::radians(pitch_deg + wobble_deg * std::sin(t * 7.3f));
    const float yaw = glm::radians(wobble_deg * std::sin(t * 4.1f + 1.0f));
    return glm::normalize(glm::vec3(std::cos(pitch) * std::sin(yaw), -std::sin(pitch),
                                    std::cos(pitch) * std::cos(yaw)));
  };
  glm::vec3 front = look(0.0f);

  std::vector<PlaceBlock> sent;
  auto first = cast(eye, front);
  REQUIRE(first);
  PlaceBlock click = placementFor(*first);
  sent.push_back(click);
  PlaceRepeat hold(click);
  hold.sent(click);

  const float dt = 1.0f / fps;
  float tick = 0.0f;
  for (float t = 0.0f; t < seconds; t += dt) {
    eye.z -= speed * dt;
    front = look(t + dt);
    for (const PlaceBlock &p : hold.sweep(dt, eye, front, kPlayerReach, cast)) sent.push_back(p);
    if ((tick += dt) >= 0.05f) { // the Game's tick: what was sent goes in
      tick -= 0.05f;
      for (const PlaceBlock &p : sent)
        if (glm::distance(eye, glm::vec3(p.pos) + 0.5f) <= kPlayerReach + 0.87f)
          cells.insert({p.pos.x, p.pos.y, p.pos.z});
      sent.clear();
    }
  }
  int z = 3;
  while (solid({0, 5, z - 1})) --z;
  return z;
}
} // namespace

TEST_CASE("flying backward holding place lays an unbroken bridge under the builder") {
  for (float fps : {30.0f, 60.0f, 144.0f}) {
    CAPTURE(fps);
    const float fly = kPlayerMoveSpeed * 3.0f;
    // Two seconds at ~13 blocks/s: ~26 blocks, the end within a couple of the builder.
    CHECK(flyBackBridging(fly, fps, 2.0f) <= -24);
    const float sprint_fly = kPlayerSprintSpeed * 5.0f;
    CHECK(flyBackBridging(sprint_fly, fps, 1.0f) <= -25);
    CHECK(flyBackBridging(kPlayerMoveSpeed, fps, 2.0f) <= -7); // walking pace
  }
}

TEST_CASE("flying backward, the row keeps going though the aim wanders off the end's side") {
  const float fly = kPlayerMoveSpeed * 3.0f;
  for (float pitch : {50.0f, 60.0f, 70.0f, 80.0f})
    for (float wobble : {0.0f, 5.0f, 10.0f}) {
      CAPTURE(pitch);
      CAPTURE(wobble);
      CHECK(flyBackBridging(fly, 60.0f, 2.0f, pitch, wobble) <= -24);
    }
}

TEST_CASE("a row's aim: past its end counts, a hold on tops has no row") {
  PlaceRepeat row(PlaceBlock{{0, 5, -1}, {0, 0, 1}}); // a row growing toward -z
  const glm::vec3 eye(0.5f, 7.6f, -3.0f);
  // Looking down into the empty cells past the end: as if at the end's side.
  auto aim = row.rowAim(eye, glm::normalize(glm::vec3(0.0f, -1.0f, 0.3f)), kPlayerReach,
                        std::nullopt);
  REQUIRE(aim);
  CHECK(aim->block_pos == glm::ivec3(0, 5, -1));
  CHECK(aim->normal == glm::ivec3(0, 0, -1));
  // Looking up at the sky: no.
  CHECK_FALSE(row.rowAim(eye, glm::vec3(0, 1, 0), kPlayerReach, std::nullopt));
  // Already on the end's side: left as it is.
  CHECK_FALSE(row.rowAim(eye, glm::vec3(0, 0, 1), kPlayerReach, hitFace({0, 5, -1}, {0, 0, -1})));

  PlaceRepeat tops(PlaceBlock{{0, 5, 0}, {0, -1, 0}});
  CHECK_FALSE(tops.rowAim(eye, glm::normalize(glm::vec3(0.0f, -1.0f, 0.3f)), kPlayerReach,
                          std::nullopt));
}
