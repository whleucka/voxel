#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

// Clouds shade the ground. Whenever the cloud mesh is rebuilt, Renderer also
// keeps a top-down map of which cloud columns hold any cloud. A point in the
// world looks toward the sun up to the band's base. If a cloud is there, most
// of its direct sunlight is taken away. block.frag does this per pixel with
// the map uploaded as a texture. Entities do it once each on the CPU with
// shadeAt(). The two must agree, so keep them in step.

// How much of the direct sun a cloud takes. Ambient light is untouched, so
// cloud shade is a little lighter than a block's shadow.
constexpr float kCloudShadowStrength = 0.85f;

// Bilinear coverage is sharpened between these, so an edge fades over a few
// blocks rather than a whole 12-block cloud cell.
constexpr float kCloudShadowEdgeLo = 0.3f;
constexpr float kCloudShadowEdgeHi = 0.7f;

// Cloud shadows fade out as the sun nears the horizon. The light is weak
// then anyway, and the long slant would put a cloud's shadow far away.
constexpr float kCloudShadowSunLo = 0.02f;
constexpr float kCloudShadowSunHi = 0.2f;

struct CloudShadowMap {
  int dim = 0;            // the map is dim × dim cloud cells
  glm::vec2 origin{0.0f}; // cloud-grid x,z of cell (0,0)'s corner (drift excluded)
  float cell = 12.0f;     // a cloud cell's width in blocks
  float base = 0.0f;      // world y of the band's underside
  std::vector<uint8_t> cover; // dim*dim, z-major rows: 255 where a column has cloud

  bool empty() const { return dim == 0; }
  float span() const { return static_cast<float>(dim) * cell; }

  // Coverage 0..1 at cloud-grid x,z. This matches a GL_LINEAR texture with
  // clamp-to-edge, whose edge cells are always clear.
  float coverAt(glm::vec2 grid) const {
    if (empty()) return 0.0f;
    glm::vec2 t = (grid - origin) / cell - 0.5f; // texel-centre space
    t = glm::clamp(t, glm::vec2(0.0f), glm::vec2(static_cast<float>(dim - 1)));
    int x0 = static_cast<int>(std::floor(t.x)), z0 = static_cast<int>(std::floor(t.y));
    int x1 = std::min(x0 + 1, dim - 1), z1 = std::min(z0 + 1, dim - 1);
    float fx = t.x - x0, fz = t.y - z0;
    auto c = [&](int x, int z) { return cover[static_cast<size_t>(z) * dim + x] / 255.0f; };
    float a = c(x0, z0) * (1 - fx) + c(x1, z0) * fx;
    float b = c(x0, z1) * (1 - fx) + c(x1, z1) * fx;
    return a * (1 - fz) + b * fz;
  }

  // The share of direct sun reaching `p` (1 is all of it, 0 is none).
  // `drift` is how far the clouds have moved along +x. `sun_dir` points
  // toward the sun.
  float shadeAt(glm::vec3 p, glm::vec3 sun_dir, float drift) const {
    if (empty() || sun_dir.y < kCloudShadowSunLo) return 1.0f;
    float t = (base - p.y) / sun_dir.y;
    if (t < 0.0f) return 1.0f; // above the band: nothing between it and the sun
    glm::vec2 at = glm::vec2(p.x, p.z) + glm::vec2(sun_dir.x, sun_dir.z) * t;
    float c = smooth(kCloudShadowEdgeLo, kCloudShadowEdgeHi, coverAt(at + glm::vec2(drift, 0.0f)));
    float fade = smooth(kCloudShadowSunLo, kCloudShadowSunHi, sun_dir.y);
    return 1.0f - c * fade * kCloudShadowStrength;
  }

private:
  static float smooth(float e0, float e1, float x) {
    float u = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
  }
};
