#include "doctest/doctest.h"
#include "net/wire.hpp"
#include "ui/title_menu.hpp"

TEST_CASE("typed addresses: host, or host:port") {
  auto a = parseAddress("192.168.1.20", 47800);
  REQUIRE(a);
  CHECK(a->host == "192.168.1.20");
  CHECK(a->port == 47800);

  auto b = parseAddress("  kitchen-pc:50000 ", 47800);
  REQUIRE(b);
  CHECK(b->host == "kitchen-pc");
  CHECK(b->port == 50000);

  CHECK_FALSE(parseAddress("", 47800));
  CHECK_FALSE(parseAddress("   ", 47800));
  CHECK_FALSE(parseAddress("host:", 47800));
  CHECK_FALSE(parseAddress("host:0", 47800));
  CHECK_FALSE(parseAddress("host:70000", 47800));
  CHECK_FALSE(parseAddress("host:12ab", 47800));
  CHECK_FALSE(parseAddress(":47800", 47800));
  CHECK_FALSE(parseAddress("two words", 47800));
}

TEST_CASE("world sizes read nicely") {
  CHECK(sizeText(0) == "0 B");
  CHECK(sizeText(1023) == "1023 B");
  CHECK(sizeText(48 * 1024) == "48 KB");
  CHECK(sizeText(3565158) == "3.4 MB");
}

TEST_CASE("the join list shows each game's mode") {
  discovery::Beacon b{wire::kProtocolVersion, "castle", "Bea", 2, 8, 47800, GameMode::Creative};
  CHECK(gameLine(b) == "castle  -  Bea's game, creative  (2/8)");
  b.mode = GameMode::Survival;
  CHECK(gameLine(b) == "castle  -  Bea's game, survival  (2/8)");
  CHECK(std::string(modeLabel(GameMode::Creative)) == "Creative");
}
