#pragma once

#include "block/block_type.hpp"
#include "ui/hud_layout.hpp"
#include <string_view>
#include <vector>

// The title drawn as a wall of blocks: a 5x7 block font where every lit cell
// is one block tile. Pure layout; the Hud draws it with the block atlas.

constexpr int kLogoGlyphW = 5;
constexpr int kLogoGlyphH = 7;
constexpr int kLogoSpaceW = 3; // a space, in cells
constexpr int kLogoGap = 1;    // cells between letters

struct LogoCell {
  int col = 0, row = 0; // row 0 at the top
  BlockType block = BlockType::STONE;
};

// The cells spelling `text` (A-Z and spaces; other characters are spaces).
// Cells with open air above are grass, the rest stone with gem ores sprinkled
// in a fixed pattern, so the logo looks the same every time.
std::vector<LogoCell> logoCells(std::string_view text);
// Width in cells.
int logoColumns(std::string_view text);

// Where the logo goes: centred, its top at 12% of the height, about 11% of
// the height tall but never wider than 84% of the width.
struct LogoLayout {
  float cell = 0;
  float x = 0, y = 0; // top-left of cell (0, 0)
  Rect bounds() const { return {x, y, cell * columns, cell * kLogoGlyphH}; }
  int columns = 0;
};
LogoLayout logoLayout(std::string_view text, float display_w, float display_h);

inline constexpr std::string_view kGameTitle = "GRUMBLEGROVE";
