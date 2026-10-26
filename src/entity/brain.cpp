#include "entity/brain.hpp"
#include "entity/entity.hpp"
#include "entity/entity_store.hpp"
#include "block/block_data.hpp"
#include "world/world.hpp"
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

  // Water or a cliff ahead: turn around instead of stepping off.
  if (opts.cautious && self.body.on_ground && !safeAhead(self, world)) {
    dir = -dir;
    yaw = std::fmod(yaw + 180.0f, 360.0f);
    in.yaw = yaw;
    // Unsafe both ways (a narrow ledge): stand still until the next decision.
    if (!safeAhead(self, world)) {
      walking = false;
      return in;
    }
  }

  in.move_dir = dir;
  if (opts.hop && self.body.on_ground) {
    in.jump = true;
    return in;
  }
  // Something solid just ahead at body height: hop over it.
  glm::vec3 ahead = self.body.position +
                    glm::vec3(dir.x, 0.0f, dir.y) * (self.body.half_width + 0.3f);
  in.jump = self.body.on_ground && self.body.collidesAt(&world, ahead);
  return in;
}

bool WanderBrain::safeAhead(const Entity &self, const World &world) const {
  glm::vec3 probe = self.body.position +
                    glm::vec3(dir.x, 0.0f, dir.y) * (self.body.half_width + 0.4f);
  int x = static_cast<int>(std::floor(probe.x));
  int z = static_cast<int>(std::floor(probe.z));
  int feet = static_cast<int>(std::floor(self.body.position.y + 0.01f));
  auto block = [&](int y) {
    return world.getBlockAt(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));
  };

  if (!opts.can_swim && isLiquid(block(feet))) return false;
  // Scan down for the ground it would land on.
  for (int down = 1; down <= kMaxSafeDrop + 1; ++down) {
    BlockType b = block(feet - down);
    if (isLiquid(b)) return opts.can_swim;
    if (b != BlockType::AIR) return true;
  }
  return false; // a drop of more than kMaxSafeDrop
}
