#include "../inc/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

void Camera::getFrustumPlanes(glm::vec4 planes[6],
                              const glm::mat4 &viewProj) const {
  // Left
  planes[0] = glm::vec4(
      viewProj[0][3] + viewProj[0][0], viewProj[1][3] + viewProj[1][0],
      viewProj[2][3] + viewProj[2][0], viewProj[3][3] + viewProj[3][0]);

  // Right
  planes[1] = glm::vec4(
      viewProj[0][3] - viewProj[0][0], viewProj[1][3] - viewProj[1][0],
      viewProj[2][3] - viewProj[2][0], viewProj[3][3] - viewProj[3][0]);

  // Bottom
  planes[2] = glm::vec4(
      viewProj[0][3] + viewProj[0][1], viewProj[1][3] + viewProj[1][1],
      viewProj[2][3] + viewProj[2][1], viewProj[3][3] + viewProj[3][1]);

  // Top
  planes[3] = glm::vec4(
      viewProj[0][3] - viewProj[0][1], viewProj[1][3] - viewProj[1][1],
      viewProj[2][3] - viewProj[2][1], viewProj[3][3] - viewProj[3][1]);

  // Near
  planes[4] = glm::vec4(
      viewProj[0][3] + viewProj[0][2], viewProj[1][3] + viewProj[1][2],
      viewProj[2][3] + viewProj[2][2], viewProj[3][3] + viewProj[3][2]);

  // Far
  planes[5] = glm::vec4(
      viewProj[0][3] - viewProj[0][2], viewProj[1][3] - viewProj[1][2],
      viewProj[2][3] - viewProj[2][2], viewProj[3][3] - viewProj[3][2]);

  // Normalize planes
  for (int i = 0; i < 6; i++) {
    float len = glm::length(glm::vec3(planes[i]));
    planes[i] /= len;
  }
}

void Camera::processMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch) {
  xoffset *= mouse_sensitivity;
  yoffset *= mouse_sensitivity;
  yaw += xoffset;   // turn head left/right
  pitch += yoffset; // look up/down

  if (constrainPitch) {
    if (pitch > 89.9f)
      pitch = 89.9f;
    if (pitch < -89.9f)
      pitch = -89.9f;
  }
  updateCameraVectors();
}
