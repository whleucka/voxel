#include "doctest/doctest.h"
#include "ui/logo.hpp"
#include <set>

TEST_CASE("logo letters are 5 cells wide with a 1-cell gap") {
  CHECK(logoColumns("A") == 5);
  CHECK(logoColumns("AB") == 11);
  CHECK(logoColumns("A B") == 5 + 1 + 3 + 1 + 5);
  CHECK(logoColumns(kGameTitle) == 12 * 5 + 11);
  CHECK(logoColumns("ab") == logoColumns("AB"));
}

TEST_CASE("logo cells spell the letters") {
  // L: a full left column and a full bottom row.
  auto l = logoCells("L");
  CHECK(l.size() == 7 + 4);
  std::set<std::pair<int, int>> at;
  for (const LogoCell &c : l) at.insert({c.col, c.row});
  for (int r = 0; r < 7; ++r) CHECK(at.count({0, r}));
  for (int c = 0; c < 5; ++c) CHECK(at.count({c, 6}));

  // The second letter starts after the first and the gap.
  for (const LogoCell &c : logoCells("IL"))
    if (c.row == 1 && c.col >= 6) CHECK(c.col == 6);
}

TEST_CASE("grass tops the letters; stone and gems fill them, the same every time") {
  auto cells = logoCells(kGameTitle);
  std::set<std::pair<int, int>> at;
  for (const LogoCell &c : cells) at.insert({c.col, c.row});
  int gems = 0;
  for (const LogoCell &c : cells) {
    bool open_above = !at.count({c.col, c.row - 1});
    if (open_above) CHECK(c.block == BlockType::GRASS);
    else CHECK(c.block != BlockType::GRASS);
    gems += c.block != BlockType::GRASS && c.block != BlockType::STONE;
  }
  CHECK(gems > 0);
  CHECK(gems < static_cast<int>(cells.size()) / 3);

  auto again = logoCells(kGameTitle);
  REQUIRE(again.size() == cells.size());
  for (size_t i = 0; i < cells.size(); ++i) CHECK(again[i].block == cells[i].block);
}

TEST_CASE("the logo fits the screen, centred near the top") {
  for (auto [w, h] : {std::pair{1920.f, 1080.f}, {1280.f, 720.f}, {1024.f, 600.f},
                      {3840.f, 2160.f}, {800.f, 1000.f}}) {
    CAPTURE(w);
    LogoLayout l = logoLayout(kGameTitle, w, h);
    Rect b = l.bounds();
    CHECK(b.x >= 0);
    CHECK(b.right() <= w);
    CHECK(std::abs((b.x + b.right()) * 0.5f - w * 0.5f) <= 1.0f);
    CHECK(b.bottom() < h * 0.35f);
    CHECK(l.cell >= 3.0f);
  }
}
