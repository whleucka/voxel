#include "doctest/doctest.h"
#include "journal/journal.hpp"
#include "ui/portrait.hpp"

TEST_CASE("every journal creature's portrait fills its rect and stays inside it") {
  const Rect r{10, 20, 64, 48};
  for (const Entry &e : entries()) {
    const auto *k = std::get_if<EntityKind>(&e.what);
    if (!k) continue;
    CAPTURE(entityType(*k).name);
    auto faces = portraitFaces(entityType(*k).model, r);
    REQUIRE_FALSE(faces.empty());
    glm::vec2 lo(1e9f), hi(-1e9f);
    for (const PortraitFace &f : faces)
      for (glm::vec2 p : f.quad.p) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
      }
    CHECK(lo.x >= r.x - 0.01f);
    CHECK(lo.y >= r.y - 0.01f);
    CHECK(hi.x <= r.right() + 0.01f);
    CHECK(hi.y <= r.bottom() + 0.01f);
    // It touches two opposite sides: as big as it can be.
    const bool wide = hi.x - lo.x > r.w - 0.1f, tall = hi.y - lo.y > r.h - 0.1f;
    CHECK((wide || tall));
  }
}

TEST_CASE("a portrait shows three faces of a box, the top brightest, and no collar") {
  ModelBox body{{-0.5f, 0, -0.5f}, {1, 1, 1}, {1, 1, 1}};
  ModelBox collar = body;
  collar.collar = true;
  auto faces = portraitFaces({body, collar}, {0, 0, 32, 32});
  REQUIRE(faces.size() == 3);
  // Drawn back to front: the top is furthest up, so its centre is drawn first
  // or last depending on the view, but it is the brightest.
  float brightest = 0;
  for (const PortraitFace &f : faces) brightest = std::max(brightest, f.color.r);
  CHECK(brightest == doctest::Approx(1.0f));
}
