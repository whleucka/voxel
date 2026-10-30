#pragma once

#include "block/block_type.hpp"
#include "core/constants.hpp"
#include "physics/physics_body.hpp"
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
  glm::vec3 getPosition() const { return body.position; }
  glm::vec3 getFeetPosition() const { return body.position; }
  glm::vec3 getEyePosition() const;
  void setPosition(glm::vec3 pos) { body.position = pos; }
  glm::vec3 getVelocity() const { return body.velocity; }

  // State queries
  bool isFlyMode() const { return fly_mode; }
  bool *getFlyModePtr() { return &fly_mode; }
  // Flying is only for worlds whose mode allows it (the Game says in Joined).
  void toggleFlyMode() { fly_mode = fly_allowed && !fly_mode; }
  void setFlyAllowed(bool allowed) {
    fly_allowed = allowed;
    fly_mode = fly_mode && allowed;
  }
  bool isFlyAllowed() const { return fly_allowed; }
  bool isUnderwater() const { return body.underwater; }  // eyes in water
  bool isInWater() const { return body.underwater || body.at_water_surface; }
  bool isOnGround() const { return body.on_ground; }
  bool isCrouching() const { return crouching; }
  bool isSprinting() const { return sprinting; }
  // The Game decides when stamina allows sprinting.
  void setSprintAllowed(bool allowed) { sprint_allowed = allowed; }
  // Put the player somewhere new (respawn) with no momentum carried over.
  void respawnAt(glm::vec3 pos);
  // Sitting in a boat: on the seat, eyes a sitting height above it. No
  // physics runs while riding; this is called every frame instead.
  void sitAt(glm::vec3 seat, float eye_height);
  // Pushed by an enemy hit (the Game's KnockedBack). Input doesn't steer for
  // a moment afterwards, so the push isn't cancelled straight away.
  void knock(glm::vec3 velocity);

  // Inventory screen and hotbar selection. The inventory itself lives in the
  // Game; the client keeps a view of it (Engine::inventory_view).
  bool isInventoryOpen() const { return inventory_open; }
  void toggleInventory() { inventory_open = !inventory_open; }
  int getSelectedHotbarSlot() const { return selected_hotbar_slot; }
  void setSelectedHotbarSlot(int slot);

private:
  // Movement modes
  bool fly_mode = false;
  bool fly_allowed = true; // tests and the default world; Joined sets it
  bool sprinting = false;
  bool sprint_allowed = true;
  bool crouching = false;
  float knockback_left = 0.0f; // seconds before input steers again
  // On a ladder: +1 climb (moving or jumping), 0 hold (crouching), -1 slide.
  int climb_input = -1;

  // Physics state (shared rules with entities)
  PhysicsBody body;

  // Camera
  Camera camera;

  // Inventory
  bool inventory_open = false;
  int selected_hotbar_slot = 0; // 0..kHotbarSlots-1

  float getPlayerHeight() const;
  float getPlayerEyeHeight() const;
};
