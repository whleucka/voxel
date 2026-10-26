#include "doctest/doctest.h"
#include "render/menu_scene.hpp"

TEST_CASE("the menu camera circles the centre at a steady distance and height") {
  glm::vec3 centre{100.0f, 70.0f, -40.0f};
  for (double t : {0.0, 10.0, 33.3, 61.0, 89.9}) {
    OrbitView v = orbitAt(centre, t);
    CHECK(v.target == centre);
    glm::vec2 flat(v.eye.x - centre.x, v.eye.z - centre.z);
    CHECK(glm::length(flat) == doctest::Approx(kOrbitRadius));
    CHECK(v.eye.y == doctest::Approx(centre.y + kOrbitHeight));
  }
}

TEST_CASE("the menu camera comes round again every period, and moves in between") {
  glm::vec3 centre{0.0f};
  OrbitView start = orbitAt(centre, 5.0);
  OrbitView lap = orbitAt(centre, 5.0 + kOrbitPeriod);
  CHECK(lap.eye.x == doctest::Approx(start.eye.x));
  CHECK(lap.eye.z == doctest::Approx(start.eye.z));
  OrbitView half = orbitAt(centre, 5.0 + kOrbitPeriod / 2);
  CHECK(half.eye.x == doctest::Approx(-start.eye.x));
  CHECK(half.eye.z == doctest::Approx(-start.eye.z));
}
