#include "render/camera_view.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

CameraView nextCameraView(CameraView view) {
  switch (view) {
    case CameraView::First: return CameraView::Behind;
    case CameraView::Behind: return CameraView::Front;
    case CameraView::Front: return CameraView::First;
  }
  return CameraView::First;
}

CameraView shownCameraView(CameraView chosen, bool in_bed, bool dead) {
  return in_bed || dead ? CameraView::First : chosen;
}

glm::vec3 cameraViewPosition(glm::vec3 eye, glm::vec3 front, CameraView view, const CameraRay &ray) {
  if (view == CameraView::First) return eye;
  const glm::vec3 out = view == CameraView::Behind ? -front : front;
  float distance = kCameraViewDistance;
  if (auto hit = ray(eye, out, kCameraViewDistance + kCameraViewMargin))
    distance = std::clamp(*hit - kCameraViewMargin, 0.0f, kCameraViewDistance);
  return eye + out * distance;
}

glm::mat4 cameraViewMatrix(glm::vec3 position, glm::vec3 front, glm::vec3 up, CameraView view) {
  const glm::vec3 look = view == CameraView::Front ? -front : front;
  return glm::lookAt(position, position + look, up);
}
