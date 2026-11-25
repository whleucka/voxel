#pragma once

// glm::perlin(vec3), bit for bit, for many samples that share a noise cell.
//
// Most of glm's classic Perlin noise goes on hashing a cell's corners into
// eight gradients, which depend only on the cell. Sampled down a column at a
// small scale (minerals, 0.05 a block), twenty blocks in a row share a cell,
// so this keeps the last cell's gradients and only blends inside it.
//
// The arithmetic is glm's (gtc/noise.inl), step for step, so the output
// matches glm::perlin exactly; the worldgen golden hashes depend on it.

#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

class CellPerlin {
public:
  float operator()(const glm::vec3 &position) {
    using glm::vec2;
    using glm::vec3;
    using glm::vec4;
    const vec3 cell = glm::floor(position);
    if (!valid_ || cell != cell_) {
      gradients(cell);
      cell_ = cell;
      valid_ = true;
    }
    const vec3 Pf0 = glm::fract(position);
    const vec3 Pf1 = Pf0 - 1.0f;

    const float n000 = glm::dot(g_[0], Pf0);
    const float n100 = glm::dot(g_[1], vec3(Pf1.x, Pf0.y, Pf0.z));
    const float n010 = glm::dot(g_[2], vec3(Pf0.x, Pf1.y, Pf0.z));
    const float n110 = glm::dot(g_[3], vec3(Pf1.x, Pf1.y, Pf0.z));
    const float n001 = glm::dot(g_[4], vec3(Pf0.x, Pf0.y, Pf1.z));
    const float n101 = glm::dot(g_[5], vec3(Pf1.x, Pf0.y, Pf1.z));
    const float n011 = glm::dot(g_[6], vec3(Pf0.x, Pf1.y, Pf1.z));
    const float n111 = glm::dot(g_[7], Pf1);

    const vec3 fade_xyz = glm::detail::fade(Pf0);
    const vec4 n_z = glm::mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111),
                              fade_xyz.z);
    const vec2 n_yz = glm::mix(vec2(n_z.x, n_z.y), vec2(n_z.z, n_z.w), fade_xyz.y);
    const float n_xyz = glm::mix(n_yz.x, n_yz.y, fade_xyz.x);
    return 2.2f * n_xyz;
  }

private:
  // The corners' gradients, normalised: g000 g100 g010 g110 g001 g101 g011 g111.
  void gradients(const glm::vec3 &floor_p) {
    using glm::vec2;
    using glm::vec3;
    using glm::vec4;
    namespace detail = glm::detail;
    vec3 Pi0 = floor_p;
    vec3 Pi1 = Pi0 + 1.0f;
    Pi0 = detail::mod289(Pi0);
    Pi1 = detail::mod289(Pi1);
    const vec4 ix(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    const vec4 iy = vec4(vec2(Pi0.y), vec2(Pi1.y));
    const vec4 iz0(Pi0.z);
    const vec4 iz1(Pi1.z);

    const vec4 ixy = detail::permute(detail::permute(ix) + iy);
    const vec4 ixy0 = detail::permute(ixy + iz0);
    const vec4 ixy1 = detail::permute(ixy + iz1);

    vec4 gx0 = ixy0 * float(1.0 / 7.0);
    vec4 gy0 = glm::fract(glm::floor(gx0) * float(1.0 / 7.0)) - 0.5f;
    gx0 = glm::fract(gx0);
    const vec4 gz0 = vec4(0.5f) - glm::abs(gx0) - glm::abs(gy0);
    const vec4 sz0 = glm::step(gz0, vec4(0.0f));
    gx0 -= sz0 * (glm::step(0.0f, gx0) - 0.5f);
    gy0 -= sz0 * (glm::step(0.0f, gy0) - 0.5f);

    vec4 gx1 = ixy1 * float(1.0 / 7.0);
    vec4 gy1 = glm::fract(glm::floor(gx1) * float(1.0 / 7.0)) - 0.5f;
    gx1 = glm::fract(gx1);
    const vec4 gz1 = vec4(0.5f) - glm::abs(gx1) - glm::abs(gy1);
    const vec4 sz1 = glm::step(gz1, vec4(0.0f));
    gx1 -= sz1 * (glm::step(0.0f, gx1) - 0.5f);
    gy1 -= sz1 * (glm::step(0.0f, gy1) - 0.5f);

    vec3 g000(gx0.x, gy0.x, gz0.x);
    vec3 g100(gx0.y, gy0.y, gz0.y);
    vec3 g010(gx0.z, gy0.z, gz0.z);
    vec3 g110(gx0.w, gy0.w, gz0.w);
    vec3 g001(gx1.x, gy1.x, gz1.x);
    vec3 g101(gx1.y, gy1.y, gz1.y);
    vec3 g011(gx1.z, gy1.z, gz1.z);
    vec3 g111(gx1.w, gy1.w, gz1.w);

    const vec4 norm0 = detail::taylorInvSqrt(
        vec4(glm::dot(g000, g000), glm::dot(g010, g010), glm::dot(g100, g100), glm::dot(g110, g110)));
    g000 *= norm0.x;
    g010 *= norm0.y;
    g100 *= norm0.z;
    g110 *= norm0.w;
    const vec4 norm1 = detail::taylorInvSqrt(
        vec4(glm::dot(g001, g001), glm::dot(g011, g011), glm::dot(g101, g101), glm::dot(g111, g111)));
    g001 *= norm1.x;
    g011 *= norm1.y;
    g101 *= norm1.z;
    g111 *= norm1.w;

    g_[0] = g000; g_[1] = g100; g_[2] = g010; g_[3] = g110;
    g_[4] = g001; g_[5] = g101; g_[6] = g011; g_[7] = g111;
  }

  glm::vec3 cell_{0.0f};
  bool valid_ = false;
  glm::vec3 g_[8];
};
