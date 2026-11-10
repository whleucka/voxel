#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <random>
#include <vector>

// What a client shows for an explosion, besides the sound: a puff of grey
// and orange cubes flying out and shrinking away, and a shake of the camera
// for anyone close. Client-only (the cubes aren't entities). Pure logic,
// tested without a GL context; EntityRenderer draws the cubes.
class BlastEffects {
public:
  static constexpr int kPuffCubes = 14;
  static constexpr float kPuffSeconds = 1.0f;   // the longest a cube lasts
  static constexpr float kPuffSpeed = 5.0f;     // blocks/s, the fastest out
  static constexpr float kShakeRange = 12.0f;   // no shake past this
  static constexpr float kShakeSeconds = 0.6f;
  static constexpr float kShakeMax = 0.3f;      // blocks the camera moves, right beside it

  struct Cube {
    glm::vec3 position{0.0f}; // centre
    glm::vec3 velocity{0.0f};
    glm::vec3 color{1.0f};    // sRGB
    float size = 0.4f;        // when new; it shrinks to nothing
    float age = 0.0f;
    float life = kPuffSeconds;
    bool glow = false;        // the fire in it: drawn unlit
    float sizeNow() const { return size * (1.0f - age / life); }
  };

  explicit BlastEffects(uint32_t seed = std::random_device{}()) : rng(seed) {}

  // Something blew up at `at`; the camera is at `camera`.
  void blast(glm::vec3 at, glm::vec3 camera);
  // Moves the cubes on, drops the spent ones, and picks this frame's shake.
  void update(float dt);

  const std::vector<Cube> &cubes() const { return cubes_; }
  // Where the camera is pushed this frame (zero when still).
  glm::vec3 shakeOffset() const { return shake_offset; }

private:
  std::mt19937 rng;
  std::vector<Cube> cubes_;
  float shake_strength = 0.0f; // blocks, at the start of the shake
  float shake_left = 0.0f;     // seconds
  glm::vec3 shake_offset{0.0f};
};
