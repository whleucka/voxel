// The client's show for an explosion: the puff of cubes and the shake.
#include "doctest/doctest.h"
#include "render/blast_effects.hpp"
#include <algorithm>
#include <cmath>

TEST_CASE("a blast puffs out cubes, some fire, that fly out, shrink and are gone in a second") {
  BlastEffects fx(3);
  const glm::vec3 at{10.0f, 64.0f, 10.0f};
  fx.blast(at, at + glm::vec3(30, 0, 0));
  REQUIRE(fx.cubes().size() == static_cast<size_t>(BlastEffects::kPuffCubes));
  int fire = 0;
  for (const auto &c : fx.cubes()) {
    fire += c.glow;
    CHECK(c.velocity.y > 0.0f); // up and out, never into the ground
    CHECK(c.sizeNow() == doctest::Approx(c.size));
  }
  CHECK(fire > 0);
  CHECK(fire < BlastEffects::kPuffCubes);

  fx.update(0.3f);
  for (const auto &c : fx.cubes()) {
    CHECK(glm::distance(c.position, at) > 0.1f);
    CHECK(c.sizeNow() < c.size);
  }
  for (int i = 0; i < 20; ++i) fx.update(0.05f);
  CHECK(fx.cubes().empty());
}

TEST_CASE("the camera shakes close by, harder nearer, and settles") {
  const glm::vec3 at{0.0f, 64.0f, 0.0f};
  auto strongest = [&](float distance) {
    BlastEffects fx(9);
    fx.blast(at, at + glm::vec3(distance, 0, 0));
    float most = 0.0f;
    for (int i = 0; i < 5; ++i) {
      fx.update(0.01f);
      const glm::vec3 o = fx.shakeOffset();
      most = std::max({most, std::abs(o.x), std::abs(o.y), std::abs(o.z)});
    }
    return most;
  };
  CHECK(strongest(BlastEffects::kShakeRange + 1.0f) == 0.0f);
  const float near = strongest(1.0f), mid = strongest(8.0f);
  CHECK(mid > 0.0f);
  CHECK(near > mid);
  CHECK(near <= BlastEffects::kShakeMax);

  BlastEffects fx(9);
  fx.blast(at, at);
  for (int i = 0; i < 20; ++i) fx.update(0.05f);
  CHECK(fx.shakeOffset() == glm::vec3(0.0f));
}
