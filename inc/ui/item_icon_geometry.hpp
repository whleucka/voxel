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

// What a block item looks like, in the HUD and dropped on the ground: a few
// boxes in cell space (0..1), each wearing a top, a side and a bottom tile.
// A plain block is one whole box; a stair, a slab, a fence, the gate, a cake
// or a cactus is its own shape.
struct IconBox {
  glm::vec3 lo{0.0f}, hi{1.0f};
  glm::ivec2 top{0}, side{0}, bottom{0};
};

struct IconModel {
  std::array<IconBox, 8> box{};
  int count = 0;
  const IconBox *begin() const { return box.data(); }
  const IconBox *end() const { return box.data() + count; }
  void add(const IconBox &b) { box[count++] = b; }
};

IconModel iconModel(BlockType type);

// One face of an icon box on screen: its quad, the atlas UVs at its four
// corners, and how bright to draw it (0..255).
struct IconFace {
  Quad quad;
  std::array<glm::vec2, 4> uv;
  int shade = 255;
};

struct IconFaces {
  std::array<IconFace, 3 * 8> face{};
  int count = 0;
  const IconFace *begin() const { return face.data(); }
  const IconFace *end() const { return face.data() + count; }
};

// The faces seen of a model in the slot (minus padding), an isometric view
// whose top face is a 2:1 rhombus, in the order to paint them: boxes
// behind first, so nearer ones cover them. Each face shows the part of its
// tile it covers, so a slab's side is the bottom half of the tile.
IconFaces iconFaces(const Rect &slot, float padding, const IconModel &model);
