#include "doctest/doctest.h"
#include "render/renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace {

const glm::vec3 kNoonSun = glm::normalize(glm::vec3(0.1f, 1.0f, 0.3f));
const glm::vec3 kHere{10.0f, 80.0f, 10.0f};

// A light space boxing the 3x3 chunks round (0, 0) and nothing beyond.
glm::mat4 boxRoundOrigin() {
  glm::mat4 view = glm::lookAt(glm::vec3(8.0f, 300.0f, 8.0f), glm::vec3(8.0f, 0.0f, 8.0f),
                               glm::vec3(0.0f, 0.0f, 1.0f));
  return glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 0.0f, 400.0f) * view;
}

ShadowRefresh drawnHere() {
  ShadowRefresh r;
  r.drawn(kNoonSun, kHere, 120.0f, 2048, boxRoundOrigin());
  return r;
}

} // namespace

TEST_CASE("the first shadow map is due") {
  ShadowRefresh r;
  CHECK(r.due(kNoonSun, kHere, 120.0f, 2048));
}

TEST_CASE("a shadow map is kept while nothing has changed enough to show") {
  ShadowRefresh r = drawnHere();
  CHECK_FALSE(r.due(kNoonSun, kHere, 120.0f, 2048));
  CHECK_FALSE(r.due(kNoonSun, kHere + glm::vec3(2.0f, 1.0f, 0.0f), 120.0f, 2048));
  glm::vec3 nudged = glm::normalize(kNoonSun + glm::vec3(0.001f, 0.0f, 0.0f));
  CHECK_FALSE(r.due(nudged, kHere, 120.0f, 2048));
}

TEST_CASE("a shadow map is drawn again when the scene it shows has moved on") {
  ShadowRefresh r = drawnHere();
  SUBCASE("the camera moved") {
    CHECK(r.due(kNoonSun, kHere + glm::vec3(ShadowRefresh::kMoveStep + 0.5f, 0, 0), 120.0f, 2048));
  }
  SUBCASE("the sun turned") {
    glm::vec3 later = glm::normalize(kNoonSun + glm::vec3(0.01f, 0.0f, 0.0f));
    CHECK(r.due(later, kHere, 120.0f, 2048));
  }
  SUBCASE("the shadow settings changed") {
    CHECK(r.due(kNoonSun, kHere, 80.0f, 2048));
    CHECK(r.due(kNoonSun, kHere, 120.0f, 4096));
  }
  SUBCASE("it was put aside (shadows off, or night)") {
    r.invalidate();
    CHECK(r.due(kNoonSun, kHere, 120.0f, 2048));
  }
}

TEST_CASE("a changed chunk redraws the shadow map only if it's in the map's box") {
  ShadowRefresh r = drawnHere();
  r.chunkChanged({5, 5}); // far outside
  CHECK_FALSE(r.due(kNoonSun, kHere, 120.0f, 2048));
  r.chunkChanged({1, 0}); // a block placed next door
  CHECK(r.due(kNoonSun, kHere, 120.0f, 2048));
  r.drawn(kNoonSun, kHere, 120.0f, 2048, boxRoundOrigin());
  CHECK_FALSE(r.due(kNoonSun, kHere, 120.0f, 2048));
}
