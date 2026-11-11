#include "doctest/doctest.h"
#include "net/send_rate.hpp"

namespace {
int sentOver(SendRate &rate, double fps, double seconds, double jitter = 0.0) {
  int sent = 0;
  int frames = static_cast<int>(fps * seconds);
  for (int i = 0; i < frames; ++i) {
    double wobble = (i % 2 ? jitter : -jitter);
    if (rate.due(i / fps + wobble)) ++sent;
  }
  return sent;
}
} // namespace

TEST_CASE("an uncapped frame rate sends at most the rate") {
  SendRate rate(60.0);
  int sent = sentOver(rate, 333.0, 10.0);
  CHECK(sent <= 601);
  CHECK(sent >= 590);
}

TEST_CASE("frames at the rate, a little jittery, all send") {
  SendRate rate(60.0);
  CHECK(sentOver(rate, 60.0, 10.0, 0.002) == 600);
}

TEST_CASE("slow frames send every frame and don't bank sends for later") {
  SendRate rate(60.0);
  CHECK(sentOver(rate, 20.0, 5.0) == 100);
  // After a long stall, one send, not a burst to catch up.
  CHECK(rate.due(100.0));
  CHECK_FALSE(rate.due(100.001));
}

TEST_CASE("the first frame sends at once, and again after a reset") {
  SendRate rate(60.0);
  CHECK(rate.due(5.0));
  CHECK_FALSE(rate.due(5.001));
  rate.reset();
  CHECK(rate.due(5.002));
}
