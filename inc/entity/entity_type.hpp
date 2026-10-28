#pragma once

#include "biome/biome_type.hpp"
#include "block/block_type.hpp"
#include "item/item_stack.hpp"
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>
#include <memory>
#include <string_view>
#include <vector>

class Brain;

// Append only: kinds are sent to clients and may later be saved.
enum class EntityKind : uint16_t {
  DroppedItem = 1,
  TestCreature = 2, // debug-panel only; exercises brains and hitting
  Woolly = 3,       // friendly animals
  Bunny = 4,
  ShellTurtle = 5,
  BlossomDeer = 6,
  Gloomling = 7,    // enemies
  ThornCrawler = 8,
  CaveGrumble = 9,
  Pebblefish = 10,  // fish
  Sunfin = 11,
  Glowfin = 12,
  Bobber = 13,      // a fishing line's float
  Zombie = 14,      // more enemies
  Mummy = 15,
};

// One cuboid of a box model, in blocks, relative to the entity's feet
// (x/z centred on the entity, y up). Facing is -Z at yaw 0.
struct ModelBox {
  glm::vec3 offset{0.0f}; // min corner
  glm::vec3 size{1.0f};
  glm::vec3 color{1.0f};  // sRGB 0..1
  int swing = 0;          // legs: +1 / -1 swing in opposite phases while moving
  bool glow = false;      // drawn at full brightness, unlit (glowing eyes)
  bool wag = false;       // tails: swing side to side about the box's front edge
};

// Friendly animals: fed, never hurt.
struct AnimalDef {
  std::vector<ItemId> eats;        // block items it takes as food
  ItemStack product;               // what it gives when fed
  std::vector<BiomeType> biomes;   // where it spawns
  std::vector<BlockType> ground;   // what it spawns standing on
};

// Night and cave creatures: they chase players and hurt them on contact.
struct EnemyDef {
  int damage = 1;                // per contact hit, on Normal
  bool surface = true;           // false: spawns in caves
  bool vanish_at_dawn = false;   // removed at sunrise when under open sky
  std::vector<BiomeType> biomes; // where it spawns; empty = anywhere
};

// Fish: swim in water, never leave it alive, can't be hurt.
struct FishDef {
  std::vector<BiomeType> biomes; // open water in these biomes (unused for cave fish)
  bool cave = false;             // true: water with a roof over it, any biome
  ItemId raw = ItemId::NONE;     // what a rod brings up
};

// A death drop that only sometimes appears.
struct ChanceDrop {
  ItemStack stack;
  float chance = 1.0f; // 0..1
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
  std::optional<AnimalDef> animal;
  std::vector<ChanceDrop> chance_drops = {};
  std::optional<EnemyDef> enemy = std::nullopt;
  std::optional<FishDef> fish = std::nullopt;
};

const EntityTypeDef &entityType(EntityKind kind);
bool isValidEntityKind(EntityKind kind);
inline bool isAnimal(EntityKind kind) {
  return isValidEntityKind(kind) && entityType(kind).animal.has_value();
}

inline bool isEnemy(EntityKind kind) {
  return isValidEntityKind(kind) && entityType(kind).enemy.has_value();
}

inline bool isFish(EntityKind kind) {
  return isValidEntityKind(kind) && entityType(kind).fish.has_value();
}

constexpr EntityKind kAnimalKinds[] = {EntityKind::Woolly, EntityKind::Bunny,
                                       EntityKind::ShellTurtle, EntityKind::BlossomDeer};
constexpr EntityKind kEnemyKinds[] = {EntityKind::Gloomling, EntityKind::ThornCrawler,
                                      EntityKind::CaveGrumble, EntityKind::Zombie,
                                      EntityKind::Mummy};
constexpr EntityKind kFishKinds[] = {EntityKind::Pebblefish, EntityKind::Sunfin,
                                     EntityKind::Glowfin};
