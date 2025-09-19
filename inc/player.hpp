#pragma once

#include "camera.hpp"
#include "world.hpp"
#include <glm/glm.hpp>

class Player {
public:
  Player(World *world, const glm::vec3 &spawn_pos);

  void update(float dt);
  void processKeyboard(bool forward, bool back, bool left, bool right,
                       bool jump, bool sprint);

  Camera &getCamera() { return camera; }
  const Camera &getCamera() const { return camera; }
  glm::vec3 getPosition() const { return position; }

private:
  // References
  World *world;
  Camera camera;

  // Movement state
  glm::vec3 position;
  glm::vec3 velocity;
  bool on_ground;
  bool in_water;

  // Constants
  float move_speed = 5.0f;
  float sprint_speed = 10.0f;
  float water_speed = 2.0f;
  float jump_strength = 8.0f;
  float gravity = -20.0f;
  float player_height = 1.8f;
  float player_radius = 0.4f;

  // Helpers
  void applyGravity(float dt);
  void handleCollisions(int axis);
};
