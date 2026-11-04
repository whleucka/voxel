#pragma once

#include "entity/entity_type.hpp"
#include "ui/hud_layout.hpp"
#include "ui/item_icon_geometry.hpp" // Quad
#include <vector>

// A creature's journal portrait: its box model seen from the front-left and
// a little above, fitted into a rect. Pure geometry, so it always matches the
// creature in the world; the Hud fills the faces.
struct PortraitFace {
  Quad quad;          // screen space
  glm::vec3 color;    // sRGB 0..1, already shaded (glowing boxes aren't)
};

// Faces back to front (draw in order). Tamed-only boxes (collars, tags) are
// left out: a portrait shows the creature as you meet it.
std::vector<PortraitFace> portraitFaces(const std::vector<ModelBox> &model, const Rect &r);
