#include "doctest/doctest.h"
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
