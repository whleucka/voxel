#include "entity/player_model.hpp"

namespace {

// The head, its bottom at height `y`: the same standing and sitting.
void addHead(std::vector<ModelBox> &m, const ResolvedLook &look, float y) {
  const glm::vec3 white{0.95f, 0.95f, 0.93f}, lash{0.06f, 0.05f, 0.06f};
  const glm::vec3 nose = look.skin * 0.88f;
  const glm::vec3 mouth = look.skin * glm::vec3(0.62f, 0.42f, 0.42f);
  m.push_back({{-0.20f, y, -0.20f}, {0.40f, 0.40f, 0.40f}, look.skin});       // head
  m.push_back({{-0.21f, y + 0.30f, -0.21f}, {0.42f, 0.12f, 0.42f}, look.hair}); // hair
  for (float x : {-0.13f, 0.04f}) {
    m.push_back({{x, y + 0.17f, -0.21f}, {0.09f, 0.07f, 0.015f}, white});     // eyes
    m.push_back({{x + 0.02f, y + 0.17f, -0.215f}, {0.05f, 0.06f, 0.01f}, look.eyes});
    m.push_back({{x, y + 0.265f, -0.21f}, {0.09f, 0.02f, 0.015f}, look.hair}); // eyebrows
  }
  m.push_back({{-0.03f, y + 0.11f, -0.225f}, {0.06f, 0.07f, 0.03f}, nose});
  m.push_back({{-0.06f, y + 0.06f, -0.21f}, {0.12f, 0.02f, 0.015f}, mouth});
  if (look.girl) {
    m.push_back({{-0.21f, y - 0.20f, 0.10f}, {0.42f, 0.62f, 0.12f}, look.hair}); // long hair, up to the top of the cap
    for (float x : {-0.225f, 0.195f}) // over the ears
      m.push_back({{x, y + 0.02f, -0.06f}, {0.03f, 0.30f, 0.18f}, look.hair});
    for (float x : {-0.21f, 0.17f}) // framing the cheeks
      m.push_back({{x, y + 0.05f, -0.215f}, {0.04f, 0.25f, 0.02f}, look.hair});
    for (float x : {-0.135f, 0.035f}) // lashes
      m.push_back({{x, y + 0.24f, -0.218f}, {0.10f, 0.012f, 0.02f}, lash});
  } else {
    m.push_back({{-0.21f, y + 0.10f, 0.10f}, {0.42f, 0.25f, 0.11f}, look.hair});
    for (float x : {-0.225f, 0.20f}) // ears
      m.push_back({{x, y + 0.13f, -0.04f}, {0.025f, 0.09f, 0.08f}, look.skin});
  }
}

// The girl's narrower body, arms and legs, or the boy's.
struct Build {
  float leg_x, leg_w, body_x, body_w, arm_x, arm_w;
};
Build buildOf(const ResolvedLook &look) {
  if (look.girl) return {0.22f, 0.21f, 0.22f, 0.44f, 0.33f, 0.11f};
  return {0.25f, 0.24f, 0.26f, 0.52f, 0.40f, 0.14f};
}

// A hand or shoe on the end of a limb, swinging about the limb's top
// (`pivot_y`) so it stays on the end of it. Limbs stop where these start:
// overlapping boxes share faces, which flicker.
ModelBox cuff(glm::vec3 offset, glm::vec3 size, glm::vec3 color, int swing, float pivot_y) {
  ModelBox box{offset, size, color, swing};
  box.pivot_y = pivot_y;
  return box;
}

} // namespace

std::vector<ModelBox> playerModel(const ResolvedLook &look) {
  const glm::vec3 pants{0.20f, 0.24f, 0.40f}, shoes{0.15f, 0.12f, 0.10f};
  const Build b = buildOf(look);
  const float right_leg = -b.leg_x + b.leg_w + 0.02f;
  std::vector<ModelBox> m = {
      {{-b.leg_x, 0.10f, -0.12f}, {b.leg_w, 0.65f, 0.24f}, pants, +1},  // legs
      {{right_leg, 0.10f, -0.12f}, {b.leg_w, 0.65f, 0.24f}, pants, -1},
      cuff({-b.leg_x, 0.00f, -0.14f}, {b.leg_w, 0.10f, 0.28f}, shoes, +1, 0.75f), // shoes
      cuff({right_leg, 0.00f, -0.14f}, {b.leg_w, 0.10f, 0.28f}, shoes, -1, 0.75f),
      {{-b.body_x, 0.75f, -0.13f}, {b.body_w, 0.65f, 0.26f}, look.shirt}, // body
      {{-b.arm_x, 0.90f, -0.12f}, {b.arm_w, 0.50f, 0.24f}, look.shirt, -1}, // arms
      {{b.body_x, 0.90f, -0.12f}, {b.arm_w, 0.50f, 0.24f}, look.shirt, +1},
      cuff({-b.arm_x, 0.78f, -0.12f}, {b.arm_w, 0.12f, 0.24f}, look.skin, -1, 1.40f), // hands
      cuff({b.body_x, 0.78f, -0.12f}, {b.arm_w, 0.12f, 0.24f}, look.skin, +1, 1.40f),
  };
  addHead(m, look, 1.40f);
  return m;
}

std::vector<ModelBox> seatedPlayerModel(const ResolvedLook &look) {
  const glm::vec3 pants{0.20f, 0.24f, 0.40f}, shoes{0.15f, 0.12f, 0.10f};
  const Build b = buildOf(look);
  const float right_leg = -b.leg_x + b.leg_w + 0.02f;
  // Everything above the hips sits 0.75 lower than standing.
  std::vector<ModelBox> m = {
      {{-b.leg_x, 0.00f, -0.38f}, {b.leg_w, 0.24f, 0.50f}, pants},  // legs, out in front
      {{right_leg, 0.00f, -0.38f}, {b.leg_w, 0.24f, 0.50f}, pants},
      cuff({-b.leg_x, 0.00f, -0.48f}, {b.leg_w, 0.28f, 0.10f}, shoes, 0, -1.0f), // shoes
      cuff({right_leg, 0.00f, -0.48f}, {b.leg_w, 0.28f, 0.10f}, shoes, 0, -1.0f),
      {{-b.body_x, 0.00f, -0.13f}, {b.body_w, 0.65f, 0.26f}, look.shirt}, // body
      {{-b.arm_x, 0.15f, -0.12f}, {b.arm_w, 0.50f, 0.24f}, look.shirt},   // arms
      {{b.body_x, 0.15f, -0.12f}, {b.arm_w, 0.50f, 0.24f}, look.shirt},
      cuff({-b.arm_x, 0.03f, -0.12f}, {b.arm_w, 0.12f, 0.24f}, look.skin, 0, -1.0f), // hands
      cuff({b.body_x, 0.03f, -0.12f}, {b.arm_w, 0.12f, 0.24f}, look.skin, 0, -1.0f),
  };
  addHead(m, look, 0.65f);
  return m;
}

std::optional<glm::vec2> projectToScreen(glm::vec3 point, const glm::mat4 &view_projection,
                                         float width, float height) {
  glm::vec4 clip = view_projection * glm::vec4(point, 1.0f);
  if (clip.w <= 1e-4f) return std::nullopt;
  glm::vec3 ndc = glm::vec3(clip) / clip.w;
  if (ndc.z > 1.0f) return std::nullopt;
  return glm::vec2((ndc.x * 0.5f + 0.5f) * width, (1.0f - (ndc.y * 0.5f + 0.5f)) * height);
}
