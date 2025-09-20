#include "player.hpp"
#include "world.hpp"

Player::Player(World *world, const glm::vec3 &spawn_pos)
    : world(world), position(spawn_pos), velocity(0.0f), on_ground(false),
      in_water(false) {
  camera.position = position;
}

void Player::update(float dt) {
  checkWater();

  // Apply gravity
  applyGravity(dt);

  // Update position based on velocity
  // Attempt to avoid jitters
  position.x += velocity.x * dt;
  handleCollisions(0); // X-axis collisions
  position.y += velocity.y * dt;
  handleCollisions(1); // Y-axis collisions
  position.z += velocity.z * dt;
  handleCollisions(2); // Z-axis collisions

  // Update camera position
  camera.position = position + glm::vec3(0.0f, player_height, 0.0f);
}

void Player::processKeyboard(bool forward, bool back, bool left, bool right,
                             bool jump, bool sprint) {
  glm::vec3 move_dir(0.0f);
  if (forward)
    move_dir += camera.front;
  if (back)
    move_dir -= camera.front;
  if (left)
    move_dir -= camera.right;
  if (right)
    move_dir += camera.right;

  // Keep movement horizontal
  move_dir.y = 0;
  if (glm::length(move_dir) > 0) {
    move_dir = glm::normalize(move_dir);
  }

  float current_speed = move_speed;

  if (in_water) {
    current_speed = water_speed;
  } else if (sprint) {
    current_speed = sprint_speed;
  }

  velocity.x = move_dir.x * current_speed;
  velocity.z = move_dir.z * current_speed;

  if (jump && on_ground) {
    velocity.y = in_water ? water_jump_strength : jump_strength;
    on_ground = false;
  }
}

void Player::applyGravity(float dt) {
  if (in_water) {
    velocity.y += gravity * 0.4f * dt;
  } else {
    velocity.y += gravity * dt;
  }
}

void Player::checkWater() {
  bool in =
      world->getBlock(position.x, position.y, position.z) == BlockType::WATER;
  bool on = world->getBlock(position.x, position.y - 1, position.z) ==
            BlockType::WATER;
  in_water = in || on;
}

void Player::handleCollisions(int axis) {
  glm::vec3 player_min = position - glm::vec3(player_radius, 0, player_radius);
  glm::vec3 player_max =
      position + glm::vec3(player_radius, player_height, player_radius);

  if (axis == 1) {
    on_ground = false;
  }

  for (int x = floor(player_min.x); x < ceil(player_max.x); x++) {
    for (int y = floor(player_min.y); y < ceil(player_max.y); y++) {
      for (int z = floor(player_min.z); z < ceil(player_max.z); z++) {
        if (world->getBlock(x, y, z) != BlockType::AIR) {
          glm::vec3 block_min(x, y, z);
          glm::vec3 block_max = block_min + glm::vec3(1.0f);

          // Check for intersection
          if ((player_max.x > block_min.x && player_min.x < block_max.x) &&
              (player_max.y > block_min.y && player_min.y < block_max.y) &&
              (player_max.z > block_min.z && player_min.z < block_max.z)) {

            if (axis == 0) { // X-axis
              if (velocity.x > 0) {
                position.x = block_min.x - player_radius;
              } else if (velocity.x < 0) {
                position.x = block_max.x + player_radius;
              }
              velocity.x = 0;
            } else if (axis == 1) { // Y-axis
              if (velocity.y > 0) {
                position.y = block_min.y - player_height;
              } else if (velocity.y < 0) {
                position.y = block_max.y;
                on_ground = true;
              }
              velocity.y = 0;
            } else if (axis == 2) { // Z-axis
              if (velocity.z > 0) {
                position.z = block_min.z - player_radius;
              } else if (velocity.z < 0) {
                position.z = block_max.z + player_radius;
              }
              velocity.z = 0;
            }
          }
        }
      }
    }
  }
}
