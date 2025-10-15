#pragma once
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum CameraMovement { FORWARD = 0, BACKWARD, LEFT, RIGHT, UP, DOWN };

class Camera {
public:
  // config
  float movement_speed = 10.0f;
  float mouse_sensitivity = 0.05f;
  float zoom = 45.0f;

  // state
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 front{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  glm::vec3 right{1.0f, 0.0f, 0.0f};
  glm::vec3 world_up{0.0f, 1.0f, 0.0f};

  float yaw = -90.0f; // -Z
  float pitch = 0.0f;

  Camera() { updateCameraVectors(); }

  glm::vec3 getPosition() const { return position; }
  void setPosition(glm::vec3 pos) { position = pos; }
  glm::vec3 getFront() { return front; }
  glm::vec3 getUp() { return up; }
  glm::vec3 getRight() { return right; }
  glm::vec3 getWorldUp() { return world_up; }

  glm::mat4 getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
  }

  void processKeyboard(CameraMovement dir, float dt) {
    float v = movement_speed * dt;
    if (dir == FORWARD)
      position += front * v;
    if (dir == BACKWARD)
      position -= front * v;
    if (dir == LEFT)
      position -= right * v;
    if (dir == RIGHT)
      position += right * v;
    if (dir == UP)
      position += world_up * v;
    if (dir == DOWN)
      position -= world_up * v;
  }

  void processMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true) {
    xoffset *= mouse_sensitivity;
    yoffset *= mouse_sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (constrainPitch) {
      if (pitch > 89.0f)
        pitch = 89.0f;
      if (pitch < -89.0f)
        pitch = -89.0f;
    }
    updateCameraVectors();
  }

  void getFrustumPlanes(glm::vec4 planes[6], const glm::mat4 &viewProj) const;

private:
  void updateCameraVectors() {
    // forward from yaw/pitch (no roll)
    glm::vec3 f;
    f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    f.y = sin(glm::radians(pitch));
    f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(f);

    // lock Right to world_up to prevent roll
    right = glm::normalize(glm::cross(front, world_up));
    // if nearly vertical, pick fallback axis
    if (glm::length(right) < 1e-6f)
      right = glm::normalize(glm::cross(front, glm::vec3(0, 0, 1)));
    up = glm::normalize(glm::cross(right, front));
  }
};
