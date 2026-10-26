#include "entity/brain.hpp"
#include "entity/entity.hpp"
#include "entity/entity_store.hpp"
#include <cmath>

Intent WanderBrain::think(const Entity &self, const World &world, Rng &rng) {
  timer_ms -= EntityStore::kTickMs;
  if (timer_ms <= 0) {
    std::uniform_int_distribution<int> percent(0, 99);
    walking = percent(rng) < 60;
    if (walking) {
      std::uniform_real_distribution<float> angle(0.0f, 2.0f * 3.14159265f);
      float a = angle(rng);
      dir = {std::sin(a), -std::cos(a)}; // yaw 0 faces -Z
      yaw = glm::degrees(a);
      timer_ms = std::uniform_int_distribution<int>(2000, 5000)(rng);
    } else {
      timer_ms = std::uniform_int_distribution<int>(1000, 3000)(rng);
    }
  }

  Intent in;
  in.yaw = yaw;
  if (!walking) return in;

  in.move_dir = dir;
  // Something solid just ahead at body height: hop over it.
  glm::vec3 ahead = self.body.position +
                    glm::vec3(dir.x, 0.0f, dir.y) * (self.body.half_width + 0.3f);
  in.jump = self.body.on_ground && self.body.collidesAt(&world, ahead);
  return in;
}
