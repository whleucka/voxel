#include "doctest/doctest.h"
#include <cmath>
#include "render/camera_view.hpp"

namespace {
const CameraRay kOpen = [](glm::vec3, glm::vec3, float) { return std::nullopt; };
const glm::vec3 kEye(10.0f, 70.0f, 10.0f), kFront(0.0f, 0.0f, -1.0f);
} // namespace

TEST_CASE("V cycles first person, behind, in front") {
  CHECK(nextCameraView(CameraView::First) == CameraView::Behind);
  CHECK(nextCameraView(CameraView::Behind) == CameraView::Front);
  CHECK(nextCameraView(CameraView::Front) == CameraView::First);
}

TEST_CASE("in bed or dead, the view is first person, and the choice comes back") {
  CHECK(shownCameraView(CameraView::Behind, true, false) == CameraView::First);
  CHECK(shownCameraView(CameraView::Front, false, true) == CameraView::First);
  CHECK(shownCameraView(CameraView::Front, false, false) == CameraView::Front);
}

TEST_CASE("in the open, the camera sits the full distance on the right side") {
  CHECK(cameraViewPosition(kEye, kFront, CameraView::First, kOpen) == kEye);
  CHECK(cameraViewPosition(kEye, kFront, CameraView::Behind, kOpen) == kEye + glm::vec3(0, 0, 4));
  CHECK(cameraViewPosition(kEye, kFront, CameraView::Front, kOpen) == kEye + glm::vec3(0, 0, -4));
}

TEST_CASE("a wall pulls the camera in, keeping it clear of the block") {
  glm::vec3 asked_direction(0.0f);
  const CameraRay wall = [&](glm::vec3 origin, glm::vec3 direction, float) -> std::optional<float> {
    CHECK(origin == kEye);
    asked_direction = direction;
    return 2.0f;
  };
  glm::vec3 at = cameraViewPosition(kEye, kFront, CameraView::Behind, wall);
  CHECK(asked_direction == -kFront);
  CHECK(at.z == doctest::Approx(kEye.z + 2.0f - kCameraViewMargin));
}

TEST_CASE("a wall against the player puts the camera at the eyes, never through it") {
  const CameraRay touching = [](glm::vec3, glm::vec3, float) { return 0.1f; };
  CHECK(cameraViewPosition(kEye, kFront, CameraView::Behind, touching) == kEye);
}

TEST_CASE("a wall just past the full distance still leaves the margin") {
  const CameraRay wall = [](glm::vec3, glm::vec3, float) { return kCameraViewDistance + 0.1f; };
  glm::vec3 at = cameraViewPosition(kEye, kFront, CameraView::Behind, wall);
  CHECK(glm::distance(at, kEye) == doctest::Approx(kCameraViewDistance + 0.1f - kCameraViewMargin));
}

TEST_CASE("the in-front view looks back at the face, even with no room") {
  const glm::vec3 up(0.0f, 1.0f, 0.0f);
  // The eyes are straight ahead of a camera in front of them.
  glm::mat4 m = cameraViewMatrix(kEye + kFront * 4.0f, kFront, up, CameraView::Front);
  glm::vec4 eye_in_view = m * glm::vec4(kEye, 1.0f);
  CHECK(eye_in_view.x == doctest::Approx(0.0f));
  CHECK(eye_in_view.z == doctest::Approx(-4.0f));
  glm::mat4 tight = cameraViewMatrix(kEye, kFront, up, CameraView::Front);
  CHECK(tight[3][3] == 1.0f); // well formed at no distance
  CHECK(!std::isnan(tight[0][0]));
}
