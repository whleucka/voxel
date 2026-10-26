#include "ui/item_icon_geometry.hpp"
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

CubeQuads cubeQuads(const Rect &slot, float padding) {
  float size = std::min(slot.w, slot.h) - 2.0f * padding;
  float half = size * 0.5f;
  float q = size * 0.25f;     // rise of the top face's edges (2:1 isometric)
  float side = size * 0.5f;   // height of the side faces; 2q + side == size
  float cx = slot.x + slot.w * 0.5f;
  float y0 = slot.y + (slot.h - size) * 0.5f;

  glm::vec2 top{cx, y0};
  glm::vec2 right{cx + half, y0 + q};
  glm::vec2 front{cx, y0 + 2 * q};
  glm::vec2 left{cx - half, y0 + q};
  glm::vec2 down{0.0f, side};

  CubeQuads c;
  c.top = {{top, right, front, left}};
  c.left = {{left, front, front + down, left + down}};
  c.right = {{front, right, right + down, front + down}};
  return c;
}
