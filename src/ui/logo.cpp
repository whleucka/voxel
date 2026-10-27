#include "ui/logo.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace {

using Glyph = std::array<const char *, kLogoGlyphH>;

// Classic 5x7 capitals.
constexpr std::array<Glyph, 26> kGlyphs = {{
    {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}, // A
    {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}, // B
    {".####", "#....", "#....", "#....", "#....", "#....", ".####"}, // C
    {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}, // D
    {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}, // E
    {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}, // F
    {".####", "#....", "#....", "#.###", "#...#", "#...#", ".###."}, // G
    {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}, // H
    {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"}, // I
    {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}, // J
    {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}, // K
    {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}, // L
    {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}, // M
    {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"}, // N
    {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}, // O
    {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}, // P
    {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}, // Q
    {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}, // R
    {".####", "#....", "#....", ".###.", "....#", "....#", "####."}, // S
    {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}, // T
    {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}, // U
    {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}, // V
    {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"}, // W
    {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}, // X
    {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}, // Y
    {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}, // Z
}};

const Glyph *glyphFor(char c) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  return c >= 'A' && c <= 'Z' ? &kGlyphs[c - 'A'] : nullptr;
}

// Gems sprinkled through the stone: about one cell in eight, by position.
BlockType stoneOrGem(int col, int row) {
  constexpr BlockType kGems[] = {BlockType::DIAMOND_ORE, BlockType::RUBY_ORE,
                                 BlockType::EMERALD_ORE, BlockType::GOLD_ORE};
  unsigned h = static_cast<unsigned>(col) * 73856093u ^ static_cast<unsigned>(row) * 19349663u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  return h % 8 == 0 ? kGems[(h / 8) % 4] : BlockType::STONE;
}

} // namespace

int logoColumns(std::string_view text) {
  int cols = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    if (i > 0) cols += kLogoGap;
    cols += glyphFor(text[i]) ? kLogoGlyphW : kLogoSpaceW;
  }
  return cols;
}

std::vector<LogoCell> logoCells(std::string_view text) {
  std::vector<LogoCell> out;
  int x = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    if (i > 0) x += kLogoGap;
    const Glyph *g = glyphFor(text[i]);
    if (!g) {
      x += kLogoSpaceW;
      continue;
    }
    for (int row = 0; row < kLogoGlyphH; ++row)
      for (int col = 0; col < kLogoGlyphW; ++col) {
        if ((*g)[row][col] != '#') continue;
        bool open_above = row == 0 || (*g)[row - 1][col] != '#';
        out.push_back({x + col, row, open_above ? BlockType::GRASS : stoneOrGem(x + col, row)});
      }
    x += kLogoGlyphW;
  }
  return out;
}

LogoLayout logoLayout(std::string_view text, float display_w, float display_h) {
  LogoLayout l;
  l.columns = std::max(1, logoColumns(text));
  float by_height = display_h * 0.11f / kLogoGlyphH;
  float by_width = display_w * 0.84f / static_cast<float>(l.columns);
  l.cell = std::floor(std::max(3.0f, std::min(by_height, by_width)));
  l.x = std::round((display_w - l.cell * static_cast<float>(l.columns)) * 0.5f);
  l.y = std::round(display_h * 0.12f);
  return l;
}
