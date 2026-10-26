#include "entity/brain.hpp"
#include "entity/entity.hpp"
#include "entity/entity_store.hpp"
#include "block/block_data.hpp"
#include "world/world.hpp"
#include <cmath>
#include <limits>

Intent WanderBrain::think(const Entity &self, const BrainContext &ctx) {
  const World &world = ctx.world;
  Rng &rng = ctx.rng;
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

// ─── Chasing ───────────────────────────────────────────────────────────

namespace {

glm::ivec3 feetCell(glm::vec3 p) {
  return {static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y + 0.01f)),
          static_cast<int>(std::floor(p.z))};
}

float horizontalDistance(glm::vec3 a, glm::vec3 b) {
  return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}

WanderOptions chaseWander(const ChaseOptions &o) {
  WanderOptions w;
  w.cautious = true;
  w.hop = o.hop;
  return w;
}

} // namespace

ChaseBrain::ChaseBrain(ChaseOptions options) : opts(options), wander(chaseWander(options)) {}

Intent ChaseBrain::think(const Entity &self, const BrainContext &ctx) {
  const glm::vec3 pos = self.body.position;
  const glm::vec3 *target = nullptr;
  float nearest = std::numeric_limits<float>::max();
  for (const glm::vec3 &t : ctx.targets) {
    float d = glm::distance(t, pos);
    if (d <= kSenseRange && d < nearest) {
      nearest = d;
      target = &t;
    }
  }
  have_target = target != nullptr;
  if (!target) {
    have_path = false;
    return wander.think(self, ctx);
  }

  replan_ms -= EntityStore::kTickMs;
  if (!have_path || replan_ms <= 0 || glm::distance(*target, planned_for) > kReplanMove) {
    PathParams params = opts.path;
    params.height_cells = std::max(1, static_cast<int>(std::ceil(self.body.height)));
    path = findPath(ctx.world, feetCell(pos), feetCell(*target), params)
               .value_or(std::vector<glm::ivec3>{});
    next = 0;
    have_path = !path.empty() || feetCell(pos) == feetCell(*target);
    planned_for = *target;
    replan_ms = kReplanMs;
  }
  // No way there: wander until the next plan.
  if (!have_path) return wander.think(self, ctx);

  while (next < path.size() &&
         horizontalDistance(pos, glm::vec3(path[next]) + glm::vec3(0.5f, 0.0f, 0.5f)) <
             kWaypointReached)
    ++next;

  // Follow the waypoints; past the last one, close in on the player.
  glm::vec3 goal = next < path.size() ? glm::vec3(path[next]) + glm::vec3(0.5f, 0.0f, 0.5f)
                                      : *target;
  glm::vec2 to(goal.x - pos.x, goal.z - pos.z);
  glm::vec2 face(target->x - pos.x, target->z - pos.z);

  Intent in;
  if (glm::length(face) > 1e-3f) in.yaw = glm::degrees(std::atan2(face.x, -face.y));
  if (glm::length(to) < 0.05f) return in;
  in.move_dir = glm::normalize(to);
  in.yaw = glm::degrees(std::atan2(in.move_dir.x, -in.move_dir.y));

  if (!self.body.on_ground) return in;
  bool climb = next < path.size() && path[next].y > feetCell(pos).y;
  glm::vec3 ahead = pos + glm::vec3(in.move_dir.x, 0.0f, in.move_dir.y) *
                              (self.body.half_width + 0.3f);
  in.jump = opts.hop || climb || self.body.collidesAt(&ctx.world, ahead);
  return in;
}
