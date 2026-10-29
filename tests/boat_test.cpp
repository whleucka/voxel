#include "doctest/doctest.h"
#include "entity/entity_store.hpp"
#include "item/item.hpp"
#include "item/recipes.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <algorithm>

namespace {

const Recipe *recipeFor(ItemId id) {
  const Recipe *found = nullptr;
  for (const Recipe &r : recipes())
    if (r.makes.id == id) {
      CHECK(found == nullptr); // just one
      found = &r;
    }
  return found;
}

} // namespace

// ─── The item, recipe and entity ───────────────────────────────────────

TEST_CASE("the Boat item and entity are pinned") {
  CHECK(static_cast<int>(EntityKind::Boat) == 16);
  CHECK(isValidItem(ItemId::BOAT));
  CHECK(itemDef(ItemId::BOAT).name == "Boat");
  CHECK(itemDef(ItemId::BOAT).max_stack == 1);
  CHECK_FALSE(itemDef(ItemId::BOAT).places); // the Game puts a boat entity down
  const auto &palette = paletteItems();
  CHECK(std::find(palette.begin(), palette.end(), ItemId::BOAT) != palette.end());
}

TEST_CASE("5 planks of any wood make a boat, anywhere") {
  const Recipe *boat = recipeFor(ItemId::BOAT);
  REQUIRE(boat);
  CHECK(boat->makes == ItemStack{ItemId::BOAT, 1});
  CHECK_FALSE(boat->needs_table);
  REQUIRE(boat->needs.size() == 1);
  CHECK(boat->needs[0].count == 5);
  // A survival player gets planks from any tree's logs.
  for (BlockType planks : {BlockType::OAK_PLANKS, BlockType::PINE_PLANKS,
                           BlockType::PALM_PLANKS, BlockType::CHERRY_PLANKS}) {
    CAPTURE(static_cast<int>(planks));
    const auto &any = boat->needs[0].any_of;
    CHECK(std::find(any.begin(), any.end(), itemForBlock(planks)) != any.end());
  }
}

TEST_CASE("a boat is a vehicle that can't be hurt") {
  const EntityTypeDef &def = entityType(EntityKind::Boat);
  CHECK(def.vehicle);
  CHECK(isVehicle(EntityKind::Boat));
  CHECK_FALSE(isVehicle(EntityKind::Woolly));
  CHECK(def.max_health == 0);
  CHECK(def.despawn_age_ms == 0);
  CHECK_FALSE(def.brain);
  CHECK_FALSE(def.model.empty());
  // Narrow enough for a one-block canal.
  CHECK(def.half_width * 2.0f < 1.0f);
}

TEST_CASE("boats stay put: never despawned, never moved by the store") {
  ScopedRenderDistance rd{0};
  World world;
  world.loadChunkSync(0, 0);
  EntityStore store;
  Rng rng{7};
  // Floating in the sky: anything stepped by physics would fall.
  const glm::vec3 at{8.0f, 200.0f, 8.0f};
  auto id = store.spawn(EntityKind::Boat, at);
  REQUIRE(id);
  // Every player is far beyond the despawn distance.
  std::vector<glm::vec3> far{{8.0f + EntityStore::kDespawnDistance * 3, 200.0f, 8.0f}};
  for (int i = 0; i < 20 * 60 * 10; ++i) store.tick(world, rng, far); // ten minutes
  REQUIRE(store.find(*id));
  CHECK(store.find(*id)->body.position == at);
}
