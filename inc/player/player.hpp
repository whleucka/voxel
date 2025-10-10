#pragma once

#include "render/camera.hpp"
#include <glm/fwd.hpp>

class Player {
public:
  Player(glm::vec3 start_pos);
  ~Player() = default;

  void update(float dt);
  void processKeyboard(float dt, bool forward, bool backward, bool left,
                       bool right, bool jump, bool sprint, bool up, bool down);
  void processMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true);
  Camera &getCamera() { return camera; }
  const Camera &getCamera() const { return camera; }
  glm::vec3 getPosition() const { return position; }
  void setPosition(glm::vec3 pos) { position = pos; }
  bool isFlyMode() const { return fly_mode; }
  bool *getFlyModePtr() { return &fly_mode; }
  void toggleFlyMode() { fly_mode = !fly_mode; }

private:
  bool fly_mode = false;
  Camera camera;
  glm::vec3 position;
  glm::vec3 velocity;

  void applyGravity(float dt);
  void checkWater();
};
