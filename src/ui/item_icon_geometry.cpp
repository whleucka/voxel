#include "ui/item_icon_geometry.hpp"
#include "block/block_data.hpp"
#include <algorithm>

UvRect tileUv(glm::ivec2 tile, int tiles_per_row) {
  float step = 1.0f / static_cast<float>(tiles_per_row);
  glm::vec2 uv0 = glm::vec2(tile) * step;
  return {uv0, uv0 + step};
}

UvRect iconUv(const ItemIcon &icon) {
  return tileUv(icon.tile, icon.atlas == ItemIcon::Atlas::Block
                               ? kBlockAtlasTiles
                               : kItemAtlasTiles);
}

// ─── Shaped icons ──────────────────────────────────────────────────────

IconModel iconModel(BlockType type) {
  constexpr float s = 1.0f / 16.0f;
  IconModel m;
  auto tex = block_data.find(type);
  if (tex == block_data.end()) return m;
  const glm::ivec2 top = tex->second.top, side = tex->second.side, bottom = tex->second.bottom;
  auto box = [&](glm::vec3 lo, glm::vec3 hi, glm::ivec2 t, glm::ivec2 sd, glm::ivec2 bt) {
    m.add({lo, hi, t, sd, bt});
  };

  if (isHalfShape(type)) {
    for (const DoorBox &b : blockBoxes(type)) box(b.lo, b.hi, top, side, bottom);
  } else if (isFence(type)) {
    // Two posts with their rails between, as a run of fence looks.
    box({s, 0, 6 * s}, {5 * s, 1, 10 * s}, side, side, side);
    box({11 * s, 0, 6 * s}, {15 * s, 1, 10 * s}, side, side, side);
    for (float y0 : {6 * s, 12 * s})
      box({5 * s, y0, kFenceRailLo}, {11 * s, y0 + 3 * s, kFenceRailHi}, side, side, side);
  } else if (isGate(type)) {
    // Shut, along X, as the mesh draws it (addGate).
    const float lo = kFenceRailLo, hi = kFenceRailHi;
    box({0, 0, lo}, {2 * s, 1, hi}, side, side, side);
    box({14 * s, 0, lo}, {1, 1, hi}, side, side, side);
    for (float y0 : {6 * s, 12 * s})
      box({2 * s, y0, lo}, {14 * s, y0 + 3 * s, hi}, side, side, side);
    box({7 * s, 6 * s, lo}, {9 * s, 15 * s, hi}, side, side, side);
  } else if (isCake(type)) {
    box({s, 0, s}, {15 * s, kCakeHeight, 15 * s}, top, side, side); // its "bottom" tile is the cut
  } else if (type == BlockType::CACTUS) {
    box({s, 0, s}, {15 * s, 1, 15 * s}, top, side, top);
  } else {
    box(glm::vec3(0.0f), glm::vec3(1.0f), top, side, bottom);
  }
  return m;
}

namespace {

// Box `a` lies wholly behind `b` along some axis: the view looks from +X,
// +Y, +Z, so lower is further away.
bool behind(const IconBox &a, const IconBox &b) {
  constexpr float eps = 1e-4f;
  for (int k = 0; k < 3; ++k)
    if (a.hi[k] <= b.lo[k] + eps) return true;
  return false;
}

} // namespace

IconFaces iconFaces(const Rect &slot, float padding, const IconModel &model) {
  // The cell's (0, 1, 0) corner is the top point, +X runs down to the right, +Z down to the left.
  float size = std::min(slot.w, slot.h) - 2.0f * padding;
  float half = size * 0.5f, q = size * 0.25f, side = size * 0.5f;
  glm::vec2 origin{slot.x + slot.w * 0.5f, slot.y + (slot.h - size) * 0.5f + side};
  glm::vec2 ex{half, q}, ez{-half, q}, ey{0.0f, -side};
  auto at = [&](float x, float y, float z) { return origin + x * ex + y * ey + z * ez; };

  // Paint order: keep taking a box that no box left is behind.
  std::array<int, 8> order{};
  std::array<bool, 8> done{};
  for (int n = 0; n < model.count; ++n) {
    int pick = -1;
    for (int i = 0; i < model.count && pick < 0; ++i) {
      if (done[i]) continue;
      bool blocked = false;
      for (int j = 0; j < model.count && !blocked; ++j)
        blocked = j != i && !done[j] && behind(model.box[j], model.box[i]) &&
                  !behind(model.box[i], model.box[j]);
      if (!blocked) pick = i;
    }
    if (pick < 0) // a cycle: shapes this simple never make one
      for (int i = 0; i < model.count && pick < 0; ++i)
        if (!done[i]) pick = i;
    done[pick] = true;
    order[n] = pick;
  }

  IconFaces out;
  for (int n = 0; n < model.count; ++n) {
    const IconBox &b = model.box[order[n]];
    const glm::vec3 lo = b.lo, hi = b.hi;
    auto uv = [](glm::ivec2 tile, float u, float v) {
      return tileUv(tile, kBlockAtlasTiles).uv0 +
             glm::vec2(u, v) / static_cast<float>(kBlockAtlasTiles);
    };
    // The +Z face (left), the +X face (right), then the top.
    out.face[out.count++] = {
        {{at(lo.x, hi.y, hi.z), at(hi.x, hi.y, hi.z), at(hi.x, lo.y, hi.z), at(lo.x, lo.y, hi.z)}},
        {{uv(b.side, lo.x, 1 - hi.y), uv(b.side, hi.x, 1 - hi.y), uv(b.side, hi.x, 1 - lo.y),
          uv(b.side, lo.x, 1 - lo.y)}},
        150};
    out.face[out.count++] = {
        {{at(hi.x, hi.y, hi.z), at(hi.x, hi.y, lo.z), at(hi.x, lo.y, lo.z), at(hi.x, lo.y, hi.z)}},
        {{uv(b.side, 1 - hi.z, 1 - hi.y), uv(b.side, 1 - lo.z, 1 - hi.y),
          uv(b.side, 1 - lo.z, 1 - lo.y), uv(b.side, 1 - hi.z, 1 - lo.y)}},
        205};
    out.face[out.count++] = {
        {{at(lo.x, hi.y, lo.z), at(hi.x, hi.y, lo.z), at(hi.x, hi.y, hi.z), at(lo.x, hi.y, hi.z)}},
        {{uv(b.top, lo.x, lo.z), uv(b.top, hi.x, lo.z), uv(b.top, hi.x, hi.z),
          uv(b.top, lo.x, hi.z)}},
        255};
  }
  return out;
}
