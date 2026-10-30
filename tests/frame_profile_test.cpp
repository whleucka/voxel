#include "doctest/doctest.h"
#include "core/frame_profile.hpp"
#include <string>
#include <thread>

TEST_CASE("a stage's time adds up within a frame and averages over frames") {
  FrameProfile p;
  CHECK(p.average(FrameProfile::Scene) == 0.0);
  p.add(FrameProfile::Scene, 1.0);
  p.add(FrameProfile::Scene, 2.0); // two passes in one frame
  p.endFrame();
  p.add(FrameProfile::Scene, 1.0);
  p.endFrame();
  CHECK(p.average(FrameProfile::Scene) == doctest::Approx(2.0));
  CHECK(p.worst(FrameProfile::Scene) == doctest::Approx(3.0));
  CHECK(p.average(FrameProfile::Game) == 0.0);
}

TEST_CASE("a hitch shows as the worst frame until it leaves the window") {
  FrameProfile p;
  p.add(FrameProfile::Uploads, 20.0);
  p.endFrame();
  for (int i = 1; i < FrameProfile::kWindow; ++i) {
    p.add(FrameProfile::Uploads, 1.0);
    p.endFrame();
  }
  CHECK(p.worst(FrameProfile::Uploads) == doctest::Approx(20.0));
  p.add(FrameProfile::Uploads, 1.0);
  p.endFrame();
  CHECK(p.worst(FrameProfile::Uploads) == doctest::Approx(1.0));
  CHECK(p.average(FrameProfile::Uploads) == doctest::Approx(1.0));
}

TEST_CASE("a scoped timer adds the time it was alive") {
  FrameProfile p;
  {
    FrameProfile::Scoped t(p, FrameProfile::Hud);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  p.endFrame();
  CHECK(p.average(FrameProfile::Hud) >= 4.0);
}

TEST_CASE("every stage has a name") {
  for (int s = 0; s < FrameProfile::kStages; ++s)
    CHECK(std::string(FrameProfile::name(static_cast<FrameProfile::Stage>(s))) != "?");
}
