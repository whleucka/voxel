#pragma once

#include "entity/brain.hpp"
#include "entity/entity_type.hpp"
#include "item/item_stack.hpp"
#include "physics/physics_body.hpp"
#include <cstdint>
#include <memory>

using EntityId = uint32_t;

// Anything in the world that isn't a block or a player. Owned and simulated by
// the Game's EntityStore; clients only ever see snapshots.
struct Entity {
  EntityId id = 0;
  EntityKind kind = EntityKind::TestCreature;
  PhysicsBody body;
  float yaw = 0.0f;
  int health = 0;
  int hurt_ms = 0;  // red flash time left
  int age_ms = 0;
  std::unique_ptr<Brain> brain;

  // Animals only
  int fed_cooldown_ms = 0;  // not hungry again until this runs out
  int happy_ms = 0;         // showing a heart
  int product_delay_ms = 0; // until the product pops out
  bool product_pending = false;

  // Dropped items only
  ItemStack item;
  int pickup_delay_ms = 0;
};
