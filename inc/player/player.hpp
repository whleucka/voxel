#pragma once

#include "block/block_type.hpp"
#include "core/constants.hpp"
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
  bool isUnderwater() const { return underwater; }  // eyes in water
  bool isInWater() const { return underwater || at_water_surface; }
  bool isOnGround() const { return on_ground; }
  bool isCrouching() const { return crouching; }
  bool isSprinting() const { return sprinting; }
  // The Game decides when stamina allows sprinting.
  void setSprintAllowed(bool allowed) { sprint_allowed = allowed; }
  // Put the player somewhere new (respawn) with no momentum carried over.
  void respawnAt(glm::vec3 pos);

  // Inventory screen and hotbar selection. The inventory itself lives in the
  // Game; the client keeps a view of it (Engine::inventory_view).
  bool isInventoryOpen() const { return inventory_open; }
  void toggleInventory() { inventory_open = !inventory_open; }
  int getSelectedHotbarSlot() const { return selected_hotbar_slot; }
  void setSelectedHotbarSlot(int slot);

private:
  // Movement modes
  bool fly_mode = false;
  bool sprinting = false;
  bool sprint_allowed = true;
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
  int selected_hotbar_slot = 0; // 0..kHotbarSlots-1

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
