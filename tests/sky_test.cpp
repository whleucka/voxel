#include "doctest/doctest.h"
#include "render/renderer.hpp"

TEST_CASE("the sun sets at the far edge of the sea, not at eye level") {
  // At the waterline, or under it, the horizon is at eye level.
  CHECK(skyHorizonY(0.0f, 320.0f) == 0.0f);
  CHECK(skyHorizonY(-20.0f, 320.0f) == 0.0f);

  // 3-4-5: 60 up, 80 out is 100 along the line of sight.
  CHECK(skyHorizonY(60.0f, 80.0f) == doctest::Approx(-0.6f));

  // The higher the eye, the lower the horizon.
  CHECK(skyHorizonY(100.0f, 320.0f) < skyHorizonY(10.0f, 320.0f));
}
