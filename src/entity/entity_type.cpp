#include "entity/entity_type.hpp"
#include "entity/brain.hpp"

namespace {

std::unique_ptr<Brain> makeWander() { return std::make_unique<WanderBrain>(); }

const EntityTypeDef kDroppedItem{
    "Dropped Item",
    /*half_width=*/0.125f, /*height=*/0.25f,
    /*max_health=*/0,
    /*despawn_age_ms=*/5 * 60 * 1000,
    /*walk_speed=*/0.0f,
    /*model=*/{}, // drawn from the item's own art
    /*brain=*/nullptr,
    /*drops=*/{},
};

// A small, clearly-placeholder critter: a body, a head and two eyes.
const EntityTypeDef kTestCreature{
    "Test Creature",
    /*half_width=*/0.35f, /*height=*/0.9f,
    /*max_health=*/10,
    /*despawn_age_ms=*/0,
    /*walk_speed=*/2.0f,
    /*model=*/{
        {{-0.35f, 0.1f, -0.30f}, {0.70f, 0.50f, 0.80f}, {0.62f, 0.36f, 0.85f}}, // body
        {{-0.25f, 0.45f, -0.60f}, {0.50f, 0.45f, 0.40f}, {0.72f, 0.48f, 0.95f}}, // head
        {{-0.18f, 0.70f, -0.62f}, {0.10f, 0.10f, 0.03f}, {0.05f, 0.05f, 0.05f}}, // eye
        {{0.08f, 0.70f, -0.62f}, {0.10f, 0.10f, 0.03f}, {0.05f, 0.05f, 0.05f}},  // eye
        {{-0.30f, 0.0f, -0.25f}, {0.15f, 0.12f, 0.15f}, {0.30f, 0.18f, 0.45f}},  // feet
        {{0.15f, 0.0f, -0.25f}, {0.15f, 0.12f, 0.15f}, {0.30f, 0.18f, 0.45f}},
        {{-0.30f, 0.0f, 0.30f}, {0.15f, 0.12f, 0.15f}, {0.30f, 0.18f, 0.45f}},
        {{0.15f, 0.0f, 0.30f}, {0.15f, 0.12f, 0.15f}, {0.30f, 0.18f, 0.45f}},
    },
    /*brain=*/makeWander,
    /*drops=*/{{ItemId::COAL, 1}},
};

} // namespace

bool isValidEntityKind(EntityKind kind) {
  return kind == EntityKind::DroppedItem || kind == EntityKind::TestCreature;
}

const EntityTypeDef &entityType(EntityKind kind) {
  switch (kind) {
    case EntityKind::DroppedItem:  return kDroppedItem;
    case EntityKind::TestCreature: return kTestCreature;
  }
  return kTestCreature;
}
