#pragma once

#include "item/item_stack.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string_view>
#include <vector>

class Brain;

// Append only: kinds are sent to clients and may later be saved.
enum class EntityKind : uint16_t {
  DroppedItem = 1,
  TestCreature = 2, // debug-panel only; exercises brains and hitting
};

// One cuboid of a box model, in blocks, relative to the entity's feet
// (x/z centred on the entity, y up). Facing is -Z at yaw 0.
struct ModelBox {
  glm::vec3 offset{0.0f}; // min corner
  glm::vec3 size{1.0f};
  glm::vec3 color{1.0f};  // sRGB 0..1
};

using BrainFactory = std::unique_ptr<Brain> (*)();

struct EntityTypeDef {
  std::string_view name;
  float half_width = 0.3f;
  float height = 1.0f;
  int max_health = 0;          // 0 = can't be hit
  int despawn_age_ms = 0;      // 0 = never by age
  float walk_speed = 0.0f;     // blocks / s for brains
  std::vector<ModelBox> model; // empty: drawn specially (dropped items)
  BrainFactory brain = nullptr;
  std::vector<ItemStack> drops;
};

const EntityTypeDef &entityType(EntityKind kind);
bool isValidEntityKind(EntityKind kind);
