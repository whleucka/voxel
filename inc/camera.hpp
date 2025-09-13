#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

/**
 * camera.hpp
 *
 * A simple camera
 * 
 */
class Camera {
public:
  // config
  float movement_speed   = 20.0f;
  float mouse_sensitivity= 0.1f;
  float zoom            = 45.0f;

  // state
  glm::vec3 position {24.0f, 56.0f, 24.0f};
  glm::vec3 front    {0.0f, 0.0f,-1.0f};
  glm::vec3 up       {0.0f, 1.0f, 0.0f};
  glm::vec3 right    {1.0f, 0.0f, 0.0f};
  glm::vec3 worldUp  {0.0f, 1.0f, 0.0f};

  float yaw   = -90.0f; // -Z
  float pitch = -30.0f;

  Camera() { updateCameraVectors(); }

  glm::vec3 getPos() {
    return position;
  }

  glm::mat4 getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
  }

  void processKeyboard(Camera_Movement dir, float dt) {
    float v = movement_speed * dt;
    if (dir == FORWARD)  position += front * v;
    if (dir == BACKWARD) position -= front * v;
    if (dir == LEFT)     position -= right * v;
    if (dir == RIGHT)    position += right * v;
    if (dir == UP)       position -= up * v;
    if (dir == DOWN)     position += up * v;
  }

  void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

  void getFrustumPlanes(glm::vec4 planes[6], float aspect, float near, float far) const;

private:
  void updateCameraVectors() {
    // forward from yaw/pitch (no roll)
    glm::vec3 f;
    f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    f.y = sin(glm::radians(pitch));
    f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(f);

    // lock Right to worldUp to prevent roll
    right = glm::normalize(glm::cross(front, worldUp));
    // if nearly vertical, pick fallback axis
    if (glm::length(right) < 1e-6f) right = glm::normalize(glm::cross(front, glm::vec3(0,0,1)));
    up = glm::normalize(glm::cross(right, front));
  }
};

