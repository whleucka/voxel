#include "entity/player_model.hpp"
#include <array>

const glm::vec3 &playerColor(std::string_view name) {
  // Bright, distinct shirts (sRGB).
  static const std::array<glm::vec3, kPlayerColors> kShirts = {{
      {0.90f, 0.25f, 0.25f}, // red
      {0.25f, 0.50f, 0.95f}, // blue
      {0.30f, 0.80f, 0.35f}, // green
      {0.98f, 0.80f, 0.20f}, // yellow
      {0.70f, 0.35f, 0.90f}, // purple
      {0.98f, 0.55f, 0.15f}, // orange
      {0.20f, 0.80f, 0.80f}, // teal
      {0.95f, 0.45f, 0.70f}, // pink
  }};
  // FNV-1a: stable across runs and machines.
  uint32_t h = 2166136261u;
  for (char c : name) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
  return kShirts[h % kPlayerColors];
}

std::vector<ModelBox> playerModel(glm::vec3 shirt) {
  const glm::vec3 skin{0.93f, 0.76f, 0.60f}, pants{0.20f, 0.24f, 0.40f},
      shoes{0.15f, 0.12f, 0.10f}, hair{0.30f, 0.20f, 0.12f}, eye{0.08f, 0.08f, 0.10f};
  return {
      {{-0.25f, 0.00f, -0.12f}, {0.24f, 0.75f, 0.24f}, pants, +1},   // legs
      {{0.01f, 0.00f, -0.12f}, {0.24f, 0.75f, 0.24f}, pants, -1},
      {{-0.25f, 0.00f, -0.14f}, {0.24f, 0.10f, 0.28f}, shoes, +1},   // shoes
      {{0.01f, 0.00f, -0.14f}, {0.24f, 0.10f, 0.28f}, shoes, -1},
      {{-0.26f, 0.75f, -0.13f}, {0.52f, 0.65f, 0.26f}, shirt},       // body
      {{-0.40f, 0.78f, -0.12f}, {0.14f, 0.62f, 0.24f}, shirt, -1},   // arms
      {{0.26f, 0.78f, -0.12f}, {0.14f, 0.62f, 0.24f}, shirt, +1},
      {{-0.40f, 0.78f, -0.12f}, {0.14f, 0.12f, 0.24f}, skin, -1},    // hands
      {{0.26f, 0.78f, -0.12f}, {0.14f, 0.12f, 0.24f}, skin, +1},
      {{-0.20f, 1.40f, -0.20f}, {0.40f, 0.40f, 0.40f}, skin},        // head
      {{-0.21f, 1.70f, -0.21f}, {0.42f, 0.12f, 0.42f}, hair},        // hair
      {{-0.21f, 1.50f, 0.10f}, {0.42f, 0.25f, 0.11f}, hair},
      {{-0.12f, 1.58f, -0.21f}, {0.07f, 0.07f, 0.02f}, eye},         // eyes
      {{0.05f, 1.58f, -0.21f}, {0.07f, 0.07f, 0.02f}, eye},
  };
}

std::optional<glm::vec2> projectToScreen(glm::vec3 point, const glm::mat4 &view_projection,
                                         float width, float height) {
  glm::vec4 clip = view_projection * glm::vec4(point, 1.0f);
  if (clip.w <= 1e-4f) return std::nullopt;
  glm::vec3 ndc = glm::vec3(clip) / clip.w;
  if (ndc.z > 1.0f) return std::nullopt;
  return glm::vec2((ndc.x * 0.5f + 0.5f) * width, (1.0f - (ndc.y * 0.5f + 0.5f)) * height);
}
