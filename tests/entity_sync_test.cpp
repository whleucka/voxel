#include "doctest/doctest.h"
#include "entity/entity_interp.hpp"
#include "physics/ray.hpp"

namespace {

EntityView view(EntityId id, glm::vec3 pos, float yaw = 0.0f) {
  EntityView v;
  v.id = id;
  v.position = pos;
  v.yaw = yaw;
  return v;
}

} // namespace

// ─── Interpolation ─────────────────────────────────────────────────────

TEST_CASE("views blend between the last two snapshots") {
  EntityInterpolator in;
  in.push({1, {view(7, {0, 0, 0}, 10.0f)}});
  in.push({2, {view(7, {2, 0, 4}, 30.0f)}});

  auto half = in.views(0.5f);
  REQUIRE(half.size() == 1);
  CHECK(half[0].position == glm::vec3(1, 0, 2));
  CHECK(half[0].yaw == doctest::Approx(20.0f));
  CHECK(in.views(0.0f)[0].position == glm::vec3(0, 0, 0));
  CHECK(in.views(1.0f)[0].position == glm::vec3(2, 0, 4));
  CHECK(in.views(3.0f)[0].position == glm::vec3(2, 0, 4)); // clamped
}

TEST_CASE("new entities appear at once; missing ones are gone") {
  EntityInterpolator in;
  in.push({1, {view(1, {0, 0, 0}), view(2, {5, 0, 5})}});
  in.push({2, {view(1, {1, 0, 0}), view(3, {9, 9, 9})}});

  auto v = in.views(0.5f);
  REQUIRE(v.size() == 2);
  CHECK(v[0].id == 1);
  CHECK(v[0].position.x == doctest::Approx(0.5f));
  CHECK(v[1].id == 3);
  CHECK(v[1].position == glm::vec3(9, 9, 9));
}

TEST_CASE("the first snapshot is shown as-is") {
  EntityInterpolator in;
  CHECK(in.views(0.5f).empty());
  in.push({1, {view(4, {3, 2, 1})}});
  auto v = in.views(0.5f);
  REQUIRE(v.size() == 1);
  CHECK(v[0].position == glm::vec3(3, 2, 1));
}

TEST_CASE("yaw blends the short way round") {
  EntityInterpolator in;
  in.push({1, {view(1, {}, 350.0f)}});
  in.push({2, {view(1, {}, 10.0f)}});
  float yaw = in.views(0.5f)[0].yaw;
  CHECK(std::fmod(yaw + 360.0f, 360.0f) == doctest::Approx(0.0f).epsilon(0.001));
}

// ─── Ray vs box ────────────────────────────────────────────────────────

TEST_CASE("a ray hits a box in front of it at the entry distance") {
  auto t = rayAabb({0, 0, 0}, {0, 0, -1}, {-1, -1, -6}, {1, 1, -4});
  REQUIRE(t);
  CHECK(*t == doctest::Approx(4.0f));
}

TEST_CASE("a ray misses boxes beside or behind it") {
  CHECK_FALSE(rayAabb({0, 0, 0}, {0, 0, -1}, {2, -1, -6}, {3, 1, -4}));
  CHECK_FALSE(rayAabb({0, 0, 0}, {0, 0, -1}, {-1, -1, 4}, {1, 1, 6}));
}

TEST_CASE("a ray starting inside a box hits at 0") {
  auto t = rayAabb({0, 0, 0}, {1, 0, 0}, {-1, -1, -1}, {1, 1, 1});
  REQUIRE(t);
  CHECK(*t == 0.0f);
}

TEST_CASE("diagonal rays hit and graze correctly") {
  auto t = rayAabb({0, 0, 0}, glm::normalize(glm::vec3(1, 1, 0)), {2, 2, -1}, {3, 3, 1});
  REQUIRE(t);
  CHECK(*t == doctest::Approx(std::sqrt(8.0f)));
  CHECK_FALSE(rayAabb({0, 0, 0}, glm::normalize(glm::vec3(1, 0.2f, 0)), {2, 2, -1}, {3, 3, 1}));
}
