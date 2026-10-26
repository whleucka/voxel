#include "doctest/doctest.h"
#include "net/discovery.hpp"
#include "net/wire.hpp"
#include <chrono>
#include <thread>

using namespace discovery;

namespace {

Beacon sample() {
  return {wire::kProtocolVersion, "world", "Host", 2, 8, 47800};
}

bool sameBeacon(const Beacon &a, const Beacon &b) {
  return a.version == b.version && a.world == b.world && a.host == b.host &&
         a.players == b.players && a.max_players == b.max_players && a.port == b.port;
}

} // namespace

TEST_CASE("beacons round-trip") {
  auto back = decode(encode(sample()));
  REQUIRE(back);
  CHECK(sameBeacon(*back, sample()));
}

TEST_CASE("bad beacons are ignored") {
  auto bytes = encode(sample());
  for (size_t n = 0; n < bytes.size(); ++n) CHECK_FALSE(decode(std::span(bytes).first(n)));
  auto padded = bytes;
  padded.push_back(0);
  CHECK_FALSE(decode(padded));
  auto wrong = bytes;
  wrong[0] = 'X';
  CHECK_FALSE(decode(wrong));
}

TEST_CASE("the browser lists games, updates them and forgets silent ones") {
  LanBrowser b;
  b.heard("192.168.1.5", sample(), 10.0);
  Beacon other = sample();
  other.host = "Dad";
  b.heard("192.168.1.9", other, 10.5);
  REQUIRE(b.games().size() == 2);

  Beacon more = sample();
  more.players = 3;
  b.heard("192.168.1.5", more, 12.0); // same game, newer news
  REQUIRE(b.games().size() == 2);
  CHECK(b.games()[0].beacon.players == 3);

  b.expire(13.6); // .9 last heard at 10.5: gone; .5 heard at 12.0: kept
  REQUIRE(b.games().size() == 1);
  CHECK(b.games()[0].address == "192.168.1.5");
  b.expire(15.1);
  CHECK(b.games().empty());
}

TEST_CASE("two games on one machine are told apart by port") {
  LanBrowser b;
  Beacon second = sample();
  second.port = 47900;
  b.heard("192.168.1.5", sample(), 1.0);
  b.heard("192.168.1.5", second, 1.0);
  CHECK(b.games().size() == 2);
}

TEST_CASE("a beacon sent over loopback is heard") {
  std::string error;
  LanBrowser browser;
  REQUIRE(browser.start(error, 0));
  BeaconSender sender;
  REQUIRE(sender.start(error, "127.0.0.1", browser.port()));
  sender.tick(0.0, sample());
  sender.tick(0.5, sample()); // too soon: nothing more is sent

  for (int i = 0; i < 200 && browser.games().empty(); ++i) {
    browser.poll(0.6);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(browser.games().size() == 1);
  CHECK(browser.games()[0].address == "127.0.0.1");
  CHECK(sameBeacon(browser.games()[0].beacon, sample()));
}
