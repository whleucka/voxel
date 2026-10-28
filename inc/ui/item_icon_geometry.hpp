#pragma once

#include "item/item.hpp"
#include "ui/hud_layout.hpp"
#include <array>
#include <glm/glm.hpp>

constexpr int kBlockAtlasTiles = 16; // 512 px / 32 px tiles
constexpr int kItemAtlasTiles = 16;  // 512 px / 32 px tiles (items use the left 8 columns)

// Texture coordinates of one atlas tile: uv0 top-left, uv1 bottom-right.
// The atlases are uploaded unflipped, so row 0 is at v = 0.
struct UvRect {
  glm::vec2 uv0{0.0f};
  glm::vec2 uv1{0.0f};
};

UvRect tileUv(glm::ivec2 tile, int tiles_per_row);
UvRect iconUv(const ItemIcon &icon);

// A screen-space quad in the order ImGui's AddImageQuad maps UVs: the points
// take the tile's top-left, top-right, bottom-right, bottom-left corners.
struct Quad {
  std::array<glm::vec2, 4> p;
};

// An isometric mini cube filling a slot (minus padding): the top face is a
// 2:1 rhombus, with the left and right side faces below it.
struct CubeQuads {
  Quad top, left, right;
};

CubeQuads cubeQuads(const Rect &slot, float padding);
