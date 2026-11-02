#include "doctest/doctest.h"
#include "entity/entity_interp.hpp"
#include "entity/player_model.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <set>

TEST_CASE("a player's colour comes from their name, and names spread out") {
  CHECK(playerColor("Bea") == playerColor("Bea"));
  std::set<std::tuple<float, float, float>> seen;
  for (const char *n : {"Bea", "Cy", "Dad", "Mum", "Ada", "Sam", "Kid_2", "Host", "Zed", "Ivy"}) {
    glm::vec3 c = playerColor(n);
    seen.insert({c.r, c.g, c.b});
  }
  CHECK(seen.size() >= 5);
  CHECK(seen.size() <= kPlayerColors);
}

TEST_CASE("the player figure stands on its feet, about a player tall") {
  auto model = playerModel(resolveLook({}, "Bea"));
  float top = 0.0f, bottom = 1.0f;
  int swinging = 0;
  for (const ModelBox &b : model) {
    top = std::max(top, b.offset.y + b.size.y);
    bottom = std::min(bottom, b.offset.y);
    swinging += b.swing != 0;
  }
  CHECK(bottom == 0.0f);
  CHECK(top == doctest::Approx(1.82f).epsilon(0.02));
  CHECK(swinging == 8); // legs, shoes, arms, hands
}

namespace {
int countColor(const std::vector<ModelBox> &model, glm::vec3 color) {
  int n = 0;
  for (const ModelBox &b : model) n += b.color == color;
  return n;
}
float width(const std::vector<ModelBox> &model) {
  float lo = 0.0f, hi = 0.0f;
  for (const ModelBox &b : model) {
    lo = std::min(lo, b.offset.x);
    hi = std::max(hi, b.offset.x + b.size.x);
  }
  return hi - lo;
}
const PlayerLook kAllChosen{0, 3, 2, 4, 2}; // green, blond, brown, blue
PlayerLook asGirl(PlayerLook look) {
  look.figure = kFigureGirl;
  return look;
}
} // namespace

TEST_CASE("the defaults keep the shirt from the name, and the old colours") {
  const ResolvedLook look = resolveLook({}, "Bea");
  CHECK(look.shirt == playerColor("Bea"));
  CHECK_FALSE(look.girl);
  CHECK(look.skin == glm::vec3(0.93f, 0.76f, 0.60f));
  CHECK(look.hair == glm::vec3(0.30f, 0.20f, 0.12f));
  CHECK(look.eyes == glm::vec3(0.08f, 0.08f, 0.10f));
}

TEST_CASE("a chosen shirt beats the name, and each part has its own colour") {
  const ResolvedLook look = resolveLook(kAllChosen, "Bea");
  CHECK(look.shirt == lookSwatches(LookPart::Shirt)[3].color);
  CHECK(look.hair == lookSwatches(LookPart::Hair)[2].color);
  CHECK(look.skin == lookSwatches(LookPart::Skin)[4].color);
  CHECK(look.eyes == lookSwatches(LookPart::Eyes)[2].color);
}

TEST_CASE("every Auto shirt colour is one of the shirt swatches") {
  auto shirts = lookSwatches(LookPart::Shirt);
  CHECK(shirts[0].name == std::string_view("auto"));
  bool found = false;
  for (size_t i = 1; i <= kPlayerColors; ++i) found |= shirts[i].color == playerColor("Bea");
  CHECK(found);
}

TEST_CASE("out-of-range values fall back to the defaults") {
  const PlayerLook bad{200, 200, 200, 200, 200};
  CHECK(sanitized(bad) == PlayerLook{});
  CHECK(sanitized(kAllChosen) == kAllChosen);
  CHECK(sanitized(asGirl(kAllChosen)) == asGirl(kAllChosen));
  CHECK(resolveLook(bad, "Bea").skin == resolveLook({}, "Bea").skin);
}

TEST_CASE("names parse back to their values") {
  for (LookPart part : {LookPart::Shirt, LookPart::Hair, LookPart::Skin, LookPart::Eyes}) {
    auto swatches = lookSwatches(part);
    for (size_t i = 0; i < swatches.size(); ++i)
      CHECK(parseSwatch(part, swatches[i].name) == static_cast<uint8_t>(i));
    CHECK_FALSE(parseSwatch(part, "plaid"));
  }
  CHECK(parseFigure("boy") == uint8_t{0});
  CHECK(parseFigure("girl") == kFigureGirl);
  CHECK_FALSE(parseFigure("robot"));
}

TEST_CASE("the face is there on both figures, standing and sitting") {
  for (PlayerLook chosen : {kAllChosen, asGirl(kAllChosen)}) {
    const ResolvedLook look = resolveLook(chosen, "Bea");
    for (const auto &model : {playerModel(look), seatedPlayerModel(look)}) {
      CHECK(countColor(model, {0.95f, 0.95f, 0.93f}) == 2); // eye whites
      CHECK(countColor(model, look.eyes) == 2);             // irises
      CHECK(countColor(model, look.skin * 0.88f) == 1);     // nose
      CHECK(countColor(model, look.skin * glm::vec3(0.62f, 0.42f, 0.42f)) == 1); // mouth
      CHECK(countColor(model, look.hair) >= 4);             // hair, eyebrows
    }
  }
}

TEST_CASE("the head is the same standing and sitting, 0.75 lower") {
  for (PlayerLook chosen : {kAllChosen, asGirl(kAllChosen)}) {
    const ResolvedLook look = resolveLook(chosen, "Bea");
    auto standing = playerModel(look), sitting = seatedPlayerModel(look);
    REQUIRE(standing.size() == sitting.size());
    for (size_t i = 9; i < standing.size(); ++i) {
      CHECK(standing[i].offset.x == sitting[i].offset.x);
      CHECK(standing[i].offset.y == doctest::Approx(sitting[i].offset.y + 0.75f));
      CHECK(standing[i].offset.z == sitting[i].offset.z);
      CHECK(standing[i].size == sitting[i].size);
      CHECK(standing[i].color == sitting[i].color);
    }
  }
}

TEST_CASE("only the girl figure has long hair and lashes, and she is narrower, not shorter") {
  const glm::vec3 lash{0.06f, 0.05f, 0.06f};
  const auto boy = playerModel(resolveLook(kAllChosen, "Bea"));
  const auto girl = playerModel(resolveLook(asGirl(kAllChosen), "Bea"));
  auto hair_bottom = [&](const std::vector<ModelBox> &model) {
    float lowest = 9.0f;
    for (const ModelBox &b : model)
      if (b.color == lookSwatches(LookPart::Hair)[2].color) lowest = std::min(lowest, b.offset.y);
    return lowest;
  };
  CHECK(countColor(boy, lash) == 0);
  CHECK(countColor(girl, lash) == 2);
  CHECK(hair_bottom(girl) < 1.40f); // below the head
  CHECK(hair_bottom(boy) >= 1.40f);
  CHECK(width(girl) < width(boy));
  auto top = [](const std::vector<ModelBox> &model) {
    float t = 0.0f;
    for (const ModelBox &b : model) t = std::max(t, b.offset.y + b.size.y);
    return t;
  };
  CHECK(top(girl) == top(boy));
  int swinging = 0;
  for (const ModelBox &b : girl) swinging += b.swing != 0;
  CHECK(swinging == 8);
}

TEST_CASE("camera yaw turns into model yaw") {
  // Camera yaw -90 looks down -Z, which is a model's yaw 0.
  CHECK(playerModelYaw(-90.0f) == 0.0f);
  CHECK(playerModelYaw(0.0f) == 90.0f); // facing +X
}

TEST_CASE("points project onto the screen, and behind the camera they don't") {
  glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
  glm::mat4 proj = glm::perspective(glm::radians(70.0f), 2.0f, 0.1f, 100.0f);
  auto centre = projectToScreen({0, 0, -10}, proj * view, 800, 400);
  REQUIRE(centre);
  CHECK(centre->x == doctest::Approx(400.0f));
  CHECK(centre->y == doctest::Approx(200.0f));
  auto above = projectToScreen({0, 3, -10}, proj * view, 800, 400);
  REQUIRE(above);
  CHECK(above->y < 200.0f); // up is up the screen
  CHECK_FALSE(projectToScreen({0, 0, 10}, proj * view, 800, 400));
}

TEST_CASE("other players are interpolated between snapshots") {
  EntityInterpolator interp;
  EntitySnapshot a, b;
  a.players = {{2, "Bea", {0, 70, 0}, 0.0f, 0.0f, false, false}};
  b.players = {{2, "Bea", {1, 70, 0}, 90.0f, 10.0f, false, false},
               {3, "Cy", {5, 70, 5}, 0.0f, 0.0f, true, false}};
  interp.push(a);
  interp.push(b);
  auto views = interp.players(0.5f);
  REQUIRE(views.size() == 2);
  CHECK(views[0].position.x == doctest::Approx(0.5f));
  CHECK(views[0].yaw == doctest::Approx(45.0f));
  CHECK(views[0].pitch == doctest::Approx(5.0f));
  CHECK(views[0].moving);
  CHECK(views[0].name == "Bea");
  CHECK(views[1].position == glm::vec3(5, 70, 5)); // new: where it is now
  CHECK_FALSE(views[1].moving);
  CHECK(views[1].crouch);
}
