#include "render/blast_effects.hpp"
#include <algorithm>
#include <cmath>

void BlastEffects::blast(glm::vec3 at, glm::vec3 camera) {
  std::uniform_real_distribution<float> unit(-1.0f, 1.0f), speed(0.4f, 1.0f),
      life(0.6f, 1.0f), size(0.3f, 0.6f), shade(0.0f, 1.0f);
  for (int i = 0; i < kPuffCubes; ++i) {
    glm::vec3 dir{unit(rng), std::abs(unit(rng)) * 0.8f + 0.2f, unit(rng)}; // mostly up and out
    dir = glm::normalize(dir);
    Cube c;
    c.position = at;
    c.velocity = dir * kPuffSpeed * speed(rng);
    c.life = kPuffSeconds * life(rng);
    c.size = size(rng);
    // One in three is fire; the rest smoke, pale to dark.
    if (i % 3 == 0) {
      c.color = glm::mix(glm::vec3(1.0f, 0.55f, 0.12f), glm::vec3(1.0f, 0.85f, 0.3f), shade(rng));
      c.glow = true;
      c.life *= 0.6f;
    } else {
      c.color = glm::vec3(glm::mix(0.35f, 0.8f, shade(rng)));
    }
    cubes_.push_back(c);
  }
  const float d = glm::distance(at, camera);
  if (d < kShakeRange) {
    const float strength = kShakeMax * (1.0f - d / kShakeRange);
    // A second blast while still shaking shakes at least as hard.
    shake_strength = std::max(strength, shake_strength * (shake_left / kShakeSeconds));
    shake_left = kShakeSeconds;
  }
}

void BlastEffects::update(float dt) {
  for (Cube &c : cubes_) {
    c.age += dt;
    c.position += c.velocity * dt;
    c.velocity *= std::exp(-3.0f * dt);     // the air slows them
    c.velocity.y += (c.glow ? 0.5f : 1.0f) * dt; // smoke drifts up
  }
  std::erase_if(cubes_, [](const Cube &c) { return c.age >= c.life; });

  shake_left = std::max(0.0f, shake_left - dt);
  if (shake_left <= 0.0f) {
    shake_offset = glm::vec3(0.0f);
    return;
  }
  std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
  shake_offset = glm::vec3(unit(rng), unit(rng), unit(rng)) * shake_strength *
                 (shake_left / kShakeSeconds);
}
