#pragma once

#include "camera.hpp"
#include "world.hpp"
#include <glm/glm.hpp>

class Player {
public:
  Player(World *world, const glm::vec3 &spawn_pos);

  // Update per frame
  void update(float dt);

  // Input
  void processKeyboard(bool forward, bool back, bool left, bool right,
                       bool jump);

  // Camera access
  Camera &getCamera() { return camera; }
  const Camera &getCamera() const { return camera; }

  // Position getters
  glm::vec3 getPosition() const { return position; }

private:
  // References
  World *world;
  Camera camera;

  // Movement state
  glm::vec3 position;
  glm::vec3 velocity;
  bool on_ground;

  // Constants
  float move_speed = 5.0f;
  float jump_strength = 8.0f;
  float gravity = -20.0f;
  float player_height = 1.8f;
  float player_radius = 0.4f;

  // Helpers
  void applyGravity(float dt);
  void handleCollisions();
};
