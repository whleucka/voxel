#include "ui/portrait.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace {

// Toward the camera: in front (-Z, where models face), to the left, above.
const glm::vec3 kToCamera = glm::normalize(glm::vec3(-0.7f, 0.55f, -1.0f));
const glm::vec3 kRight = glm::normalize(glm::cross(-kToCamera, glm::vec3(0, 1, 0)));
const glm::vec3 kUp = glm::cross(kRight, -kToCamera);

struct Face3 {
  std::array<glm::vec3, 4> p;
  glm::vec3 color;
  float depth; // larger is nearer the camera
};

} // namespace

std::vector<PortraitFace> portraitFaces(const std::vector<ModelBox> &model, const Rect &r) {
  std::vector<Face3> faces;
  for (const ModelBox &b : model) {
    if (b.collar || b.collar_tag) continue;
    const glm::vec3 lo = b.offset, hi = b.offset + b.size;
    // The three faces turned toward the camera: front, left side, top.
    auto add = [&](std::array<glm::vec3, 4> p, glm::vec3 normal, float shade) {
      if (glm::dot(normal, kToCamera) <= 0.0f) return;
      glm::vec3 mid = (p[0] + p[1] + p[2] + p[3]) * 0.25f;
      faces.push_back({p, b.glow ? b.color : b.color * shade, glm::dot(mid, kToCamera)});
    };
    add({glm::vec3(lo.x, hi.y, lo.z), {hi.x, hi.y, lo.z}, {hi.x, lo.y, lo.z}, {lo.x, lo.y, lo.z}},
        {0, 0, -1}, 0.85f);
    add({glm::vec3(lo.x, hi.y, hi.z), {lo.x, hi.y, lo.z}, {lo.x, lo.y, lo.z}, {lo.x, lo.y, hi.z}},
        {-1, 0, 0}, 0.65f);
    add({glm::vec3(lo.x, hi.y, hi.z), {hi.x, hi.y, hi.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z}},
        {0, 1, 0}, 1.0f);
  }
  std::stable_sort(faces.begin(), faces.end(),
                   [](const Face3 &a, const Face3 &b) { return a.depth < b.depth; });

  // Project, then fit the whole picture into the rect, centred.
  constexpr float kInf = std::numeric_limits<float>::max();
  glm::vec2 lo(kInf), hi(-kInf);
  std::vector<PortraitFace> out;
  out.reserve(faces.size());
  for (const Face3 &f : faces) {
    PortraitFace pf{{}, f.color};
    for (int i = 0; i < 4; ++i) {
      glm::vec2 s(glm::dot(f.p[i], kRight), -glm::dot(f.p[i], kUp));
      pf.quad.p[i] = s;
      lo = glm::min(lo, s);
      hi = glm::max(hi, s);
    }
    out.push_back(pf);
  }
  if (out.empty()) return out;
  const glm::vec2 extent = glm::max(hi - lo, glm::vec2(1e-4f));
  const float scale = std::min(r.w / extent.x, r.h / extent.y);
  const glm::vec2 offset = r.center() - (lo + hi) * 0.5f * scale;
  for (PortraitFace &pf : out)
    for (glm::vec2 &p : pf.quad.p) p = p * scale + offset;
  return out;
}
