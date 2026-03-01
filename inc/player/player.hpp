#pragma once

#include "block/block_type.hpp"
#include "render/camera.hpp"
#include <array>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>

class World;

struct PlayerInput {
  bool forward = false;
  bool backward = false;
  bool left = false;
  bool right = false;
  bool jump = false;
  bool sprint = false;
  bool crouch = false;
  // Fly mode controls
  bool fly_up = false;
  bool fly_down = false;
};

class Player {
public:
  Player(glm::vec3 start_pos);
  ~Player() = default;

  void update(float dt, World* world);
  void processKeyboard(float dt, const PlayerInput& input);
  void processMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true);
  Camera &getCamera() { return camera; }
  const Camera &getCamera() const { return camera; }
  glm::vec3 getPosition() const { return position; }
  glm::vec3 getFeetPosition() const { return position; }
  glm::vec3 getEyePosition() const;
  void setPosition(glm::vec3 pos) { position = pos; }
  glm::vec3 getVelocity() const { return velocity; }

  // State queries
  bool isFlyMode() const { return fly_mode; }
  bool *getFlyModePtr() { return &fly_mode; }
  void toggleFlyMode() { fly_mode = !fly_mode; }
  bool isUnderwater() const { return underwater; }
  bool isOnGround() const { return on_ground; }
  bool isCrouching() const { return crouching; }
  bool isSprinting() const { return sprinting; }

  // Inventory / hotbar
  bool isInventoryOpen() const { return inventory_open; }
  void toggleInventory() { inventory_open = !inventory_open; }
  int getSelectedHotbarSlot() const { return selected_hotbar_slot; }
  void setSelectedHotbarSlot(int slot);
  BlockType getHotbarBlock() const;
  BlockType getHotbarBlock(int slot) const;

private:
  // Movement modes
  bool fly_mode = false;
  bool sprinting = false;
  bool crouching = false;

  // Physics state
  bool on_ground = false;
  bool underwater = false;
  bool at_water_surface = false;
  glm::vec3 position{0.0f};
  glm::vec3 velocity{0.0f};

  // Camera
  Camera camera;

  // Inventory
  bool inventory_open = false;
  int selected_hotbar_slot = 0; // 0-8
  std::array<BlockType, 9> hotbar_blocks;

  // Internal methods
  void applyGravity(float dt);
  void checkWater(World* world);
  void resolveCollisions(World* world, glm::vec3 old_pos);

  // Collision helpers
  bool isSolidAt(World* world, int bx, int by, int bz) const;
  bool collidesWithWorld(World* world, glm::vec3 pos) const;
  bool tryAutoStep(World* world, glm::vec3& pos, float old_axis, int axis) const;
  float getPlayerHeight() const;
  float getPlayerEyeHeight() const;
};
