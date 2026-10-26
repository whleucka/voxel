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
  auto model = playerModel(playerColor("Bea"));
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
