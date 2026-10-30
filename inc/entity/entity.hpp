#pragma once

#include "entity/brain.hpp"
#include "entity/entity_type.hpp"
#include "item/item_stack.hpp"
#include "physics/physics_body.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

using EntityId = uint32_t;

// Whose pet it is: the host, or a guest by name (names outlast PlayerIds).
struct PetOwner {
  bool host = true;
  std::string name; // guests only
  uint8_t collar = 1; // 1..kCollarColours
  bool sitting = false;
};

// Anything in the world that isn't a block or a player. Owned and simulated by
// the Game's EntityStore; clients only ever see snapshots.
struct Entity {
  EntityId id = 0;
  EntityKind kind = EntityKind::TestCreature;
  PhysicsBody body;
  float yaw = 0.0f;
  int health = 0;
  int hurt_ms = 0;  // red flash time left
  int burning_ms = 0;   // set alight (a ruby sword): time left
  int burn_tick_ms = 0; // toward the next point of burn damage
  int age_ms = 0;
  int attack_cooldown_ms = 0; // enemies: until the next contact hit
  std::unique_ptr<Brain> brain;

  // Animals only
  int fed_cooldown_ms = 0;  // not hungry again until this runs out
  int happy_ms = 0;         // showing a heart
  int product_delay_ms = 0; // until the product pops out
  bool product_pending = false;

  // Pets only
  int treats = 0;                   // strays: toward taming
  std::optional<PetOwner> pet;      // set once tamed
  std::optional<glm::vec3> follow;  // the owner's feet, set by the Game each tick

  // Fish only
  int stranded_ms = 0; // out of water this long; it vanishes at kStrandedMs

  // Bobbers only
  uint32_t owner = 0;  // the PlayerId fishing with it
  bool biting = false; // dipped under: reel in now

  // Dropped items only
  ItemStack item;
  int pickup_delay_ms = 0;
};
