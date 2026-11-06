#include "doctest/doctest.h"
#include "render/cloud_shadow.hpp"

namespace {
// A 5×5 map of 12-block cells from (0,0), base at y 200, with cloud over the
// middle 3×3 cells (blocks 12..48 on x and z). The outer ring stays clear,
// as the Renderer leaves it.
CloudShadowMap map() {
  CloudShadowMap m;
  m.dim = 5;
  m.cell = 12.0f;
  m.base = 200.0f;
  m.cover.assign(25, 0);
  for (int z = 1; z < 4; ++z)
    for (int x = 1; x < 4; ++x) m.cover[z * 5 + x] = 255;
  return m;
}
const glm::vec3 kNoon{0.0f, 1.0f, 0.0f};
} // namespace

TEST_CASE("no map, no shade") {
  CloudShadowMap m;
  CHECK(m.shadeAt({30, 70, 30}, kNoon, 0.0f) == 1.0f);
}

TEST_CASE("under a cloud at noon most of the sun is gone; in the clear it's all there") {
  CloudShadowMap m = map();
  CHECK(m.shadeAt({30, 70, 30}, kNoon, 0.0f) == doctest::Approx(1.0f - kCloudShadowStrength));
  CHECK(m.shadeAt({2, 70, 2}, kNoon, 0.0f) == 1.0f);
  CHECK(m.shadeAt({500, 70, 500}, kNoon, 0.0f) == 1.0f); // off the map: clear
}

TEST_CASE("the edge of a cloud's shadow fades over a few blocks") {
  CloudShadowMap m = map();
  float in = m.shadeAt({20, 70, 30}, kNoon, 0.0f);
  float edge = m.shadeAt({12, 70, 30}, kNoon, 0.0f);
  float out = m.shadeAt({4, 70, 30}, kNoon, 0.0f);
  CHECK(in < edge);
  CHECK(edge < out);
}

TEST_CASE("a slanting sun throws the shadow away from under the cloud") {
  CloudShadowMap m = map();
  glm::vec3 sun = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)); // 45°, toward +x
  // 100 below the base: the ray climbs to x+100, which is past the cloud.
  CHECK(m.shadeAt({30, 100, 30}, sun, 0.0f) == 1.0f);
  // Out west of the cloud on the ground, the cloud is overhead along the ray.
  CHECK(m.shadeAt({-70, 100, 30}, sun, 0.0f) < 0.5f);
}

TEST_CASE("the shadow drifts with the clouds") {
  CloudShadowMap m = map();
  // Clouds drift along +x: a drift of 24 puts cloud-grid x 30 over world x 6.
  CHECK(m.shadeAt({6, 70, 30}, kNoon, 24.0f) < 0.5f);
  CHECK(m.shadeAt({45, 70, 30}, kNoon, 24.0f) == 1.0f);
}

TEST_CASE("no cloud shadows above the band, at night or with the sun on the horizon") {
  CloudShadowMap m = map();
  CHECK(m.shadeAt({30, 230, 30}, kNoon, 0.0f) == 1.0f);
  CHECK(m.shadeAt({30, 70, 30}, {0.0f, -1.0f, 0.0f}, 0.0f) == 1.0f);
  CHECK(m.shadeAt({30, 70, 30}, glm::normalize(glm::vec3(1.0f, 0.01f, 0.0f)), 0.0f) == 1.0f);
}
