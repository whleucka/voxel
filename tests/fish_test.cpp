#include "doctest/doctest.h"
#include "biome/biome_manager.hpp"
#include "entity/entity_store.hpp"
#include "entity/fish.hpp"
#include "item/item.hpp"
#include "item/recipes.hpp"
#include "item/smelting.hpp"
#include "item/tools.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <algorithm>

// ─── Items ─────────────────────────────────────────────────────────────

TEST_CASE("the fishing rod is one to a stack and lasts 64 catches") {
  CHECK(itemDef(ItemId::FISHING_ROD).max_stack == 1);
  CHECK(usesOf(ItemId::FISHING_ROD) == 64);
  // Not a mining tool: hand speed, and it hits like a hand.
  CHECK(toolOf(ItemId::FISHING_ROD).kind == ToolKind::None);
  CHECK(attackDamage(ItemId::FISHING_ROD) == 1);
}

TEST_CASE("usesOf covers tools and nothing else") {
  CHECK(usesOf(ItemId::DIAMOND_PICKAXE) == maxUses(Tier::Diamond));
  CHECK(usesOf(ItemId::WOOD_SWORD) == maxUses(Tier::Wood));
  CHECK(usesOf(ItemId::NONE) == 0);
  CHECK(usesOf(ItemId::COAL) == 0);
  CHECK(usesOf(ItemId::SOGGY_BOOT) == 0);
}

TEST_CASE("raw fish heal a little, cooked fish more, and the boot isn't food") {
  CHECK(itemDef(ItemId::RAW_PEBBLEFISH).food_heal == 2);
  CHECK(itemDef(ItemId::RAW_SUNFIN).food_heal == 2);
  CHECK(itemDef(ItemId::RAW_GLOWFIN).food_heal == 2);
  CHECK(itemDef(ItemId::COOKED_PEBBLEFISH).food_heal == 6);
  CHECK(itemDef(ItemId::COOKED_SUNFIN).food_heal == 6);
  CHECK(itemDef(ItemId::COOKED_GLOWFIN).food_heal == 8);
  CHECK(itemDef(ItemId::SOGGY_BOOT).food_heal == 0);
  CHECK(itemDef(ItemId::SOGGY_BOOT).max_stack == 1);
}

TEST_CASE("the rod is 3 sticks and 2 wool at a table") {
  int found = 0;
  for (const Recipe &r : recipes()) {
    if (r.makes.id != ItemId::FISHING_ROD) continue;
    ++found;
    CHECK(r.needs_table);
    CHECK(r.makes.count == 1);
    REQUIRE(r.needs.size() == 2);
    CHECK(r.needs[0].any_of == std::vector<ItemId>{ItemId::STICKS});
    CHECK(r.needs[0].count == 3);
    CHECK(r.needs[1].any_of == std::vector<ItemId>{ItemId::WOOL});
    CHECK(r.needs[1].count == 2);
  }
  CHECK(found == 1);
}

TEST_CASE("each raw fish cooks into its cooked fish, and cooked fish don't cook again") {
  CHECK(smeltResult(ItemId::RAW_PEBBLEFISH) == ItemId::COOKED_PEBBLEFISH);
  CHECK(smeltResult(ItemId::RAW_SUNFIN) == ItemId::COOKED_SUNFIN);
  CHECK(smeltResult(ItemId::RAW_GLOWFIN) == ItemId::COOKED_GLOWFIN);
  for (ItemId id : {ItemId::COOKED_PEBBLEFISH, ItemId::COOKED_SUNFIN, ItemId::COOKED_GLOWFIN,
                    ItemId::SOGGY_BOOT, ItemId::FISHING_ROD})
    CHECK_FALSE(smeltResult(id));
}

// ─── Fish creatures ────────────────────────────────────────────────────

namespace {

constexpr int kBase = 200; // clear sky above chunk (0,0)

// A fish in a hand-built tank in the sky over chunk (0, 0).
struct Tank {
  World world;
  EntityStore store;
  Rng rng{91};
  std::vector<glm::vec3> players{{8.0f, kBase + 1.0f, 8.0f}};
  EntityId id = 0;

  // Water fills [lo, hi]; stone walls, floor and lid around it.
  Tank(glm::ivec3 lo, glm::ivec3 hi) {
    world.loadChunkSync(0, 0);
    fill(world, lo - glm::ivec3(1), hi + glm::ivec3(1), BlockType::STONE);
    fill(world, lo, hi, BlockType::WATER);
  }
  void spawn(EntityKind kind, glm::vec3 at) {
    auto e = store.spawn(kind, at);
    REQUIRE(e);
    id = *e;
  }
  const Entity *e() const { return store.find(id); }
  void tick() { store.tick(world, rng, players); }
};

} // namespace

TEST_CASE("the three fish are swimmers that can't be hurt or fed") {
  REQUIRE(std::size(kFishKinds) == 3);
  for (EntityKind k : kFishKinds) {
    const EntityTypeDef &def = entityType(k);
    CAPTURE(def.name);
    CHECK(isFish(k));
    CHECK_FALSE(isAnimal(k));
    CHECK_FALSE(isEnemy(k));
    CHECK(def.max_health == 0);
    CHECK(def.brain != nullptr);
    CHECK_FALSE(def.model.empty());
    CHECK(std::any_of(def.model.begin(), def.model.end(), [](const ModelBox &b) { return b.wag; }));
  }
  CHECK(entityType(EntityKind::Pebblefish).fish->raw == ItemId::RAW_PEBBLEFISH);
  CHECK(entityType(EntityKind::Sunfin).fish->raw == ItemId::RAW_SUNFIN);
  CHECK(entityType(EntityKind::Glowfin).fish->raw == ItemId::RAW_GLOWFIN);
  CHECK(entityType(EntityKind::Glowfin).fish->cave);
  const auto &glow = entityType(EntityKind::Glowfin).model;
  CHECK(std::any_of(glow.begin(), glow.end(), [](const ModelBox &b) { return b.glow; }));
  CHECK_FALSE(isFish(EntityKind::Woolly));
  CHECK(static_cast<uint16_t>(EntityKind::Pebblefish) == 10);
  CHECK(static_cast<uint16_t>(EntityKind::Glowfin) == 12);
}

TEST_CASE("a fish swims about its tank and never leaves the water") {
  Tank t({4, kBase, 4}, {11, kBase + 3, 11});
  t.spawn(EntityKind::Sunfin, {8.0f, kBase + 1.5f, 8.0f});
  glm::vec3 start = t.e()->body.position;
  float moved = 0.0f;
  float lowest = 1e9f, highest = -1e9f;
  for (int i = 0; i < 20 * 120; ++i) {
    t.tick();
    REQUIRE(t.e());
    const PhysicsBody &b = t.e()->body;
    REQUIRE(SwimBrain::waterAhead(*t.e(), t.world, glm::vec3(0.0f), 0.0f));
    moved = std::max(moved, glm::distance(start, b.position));
    lowest = std::min(lowest, b.position.y);
    highest = std::max(highest, b.position.y);
  }
  CHECK(moved > 2.0f);           // it really swims
  CHECK(highest - lowest > 0.3f); // up and down too
}

TEST_CASE("a fish in open-topped water stays under the surface") {
  // A pond with air above: the fish must not jump out.
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {3, kBase - 1, 3}, {12, kBase - 1, 12}, BlockType::STONE);
  fill(world, {3, kBase, 3}, {12, kBase + 2, 12}, BlockType::STONE);
  fill(world, {4, kBase, 4}, {11, kBase + 2, 11}, BlockType::WATER);
  EntityStore store;
  Rng rng{5};
  std::vector<glm::vec3> players{{8.0f, kBase + 1.0f, 8.0f}};
  auto id = store.spawn(EntityKind::Pebblefish, {8.0f, kBase + 1.0f, 8.0f});
  REQUIRE(id);
  for (int i = 0; i < 20 * 120; ++i) {
    store.tick(world, rng, players);
    REQUIRE(store.find(*id));
    REQUIRE(store.find(*id)->body.position.y + store.find(*id)->body.height < kBase + 3.0f);
  }
}

TEST_CASE("a stranded fish flops for 3 s and vanishes") {
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {2, kBase, 2}, {13, kBase, 13}, BlockType::STONE);
  EntityStore store;
  Rng rng{3};
  std::vector<glm::vec3> players{{8.0f, kBase + 1.0f, 8.0f}};
  auto id = store.spawn(EntityKind::Pebblefish, {8.0f, kBase + 1.0f, 8.0f});
  REQUIRE(id);
  for (int ms = 0; ms < EntityStore::kStrandedMs - 2 * EntityStore::kTickMs;
       ms += EntityStore::kTickMs)
    store.tick(world, rng, players);
  CHECK(store.find(*id));
  for (int i = 0; i < 4; ++i) store.tick(world, rng, players);
  CHECK_FALSE(store.find(*id));
}

TEST_CASE("a fish back in water stops counting down") {
  Tank t({4, kBase, 4}, {11, kBase + 2, 11});
  t.spawn(EntityKind::Pebblefish, {8.0f, kBase + 1.0f, 8.0f});
  t.store.find(t.id)->stranded_ms = EntityStore::kStrandedMs - 100;
  t.tick();
  REQUIRE(t.e());
  CHECK(t.e()->stranded_ms == 0);
}

// ─── Spawning ──────────────────────────────────────────────────────────

TEST_CASE("waterAt finds water at least 2 deep, and nothing on dry land") {
  World world;
  world.loadChunkSync(0, 0);
  Rng rng{1};
  fill(world, {0, 0, 0}, {15, kChunkHeight - 1, 15}, BlockType::AIR);
  fill(world, {2, kBase, 2}, {2, kBase, 2}, BlockType::STONE);      // dry
  fill(world, {3, kBase, 3}, {3, kBase, 3}, BlockType::WATER);      // 1 deep
  fill(world, {4, kBase, 4}, {4, kBase + 2, 4}, BlockType::WATER);  // 3 deep
  CHECK_FALSE(Spawner::waterAt(world, 2, 2, rng));
  CHECK_FALSE(Spawner::waterAt(world, 3, 3, rng));
  for (int i = 0; i < 20; ++i) {
    auto c = Spawner::waterAt(world, 4, 4, rng);
    REQUIRE(c);
    CHECK(c->y >= kBase);
    CHECK(c->y <= kBase + 2);
  }
  CHECK_FALSE(Spawner::waterAt(world, 1000, 1000, rng)); // unloaded
}

TEST_CASE("each fish spawns only in its own water") {
  // Open water in each biome, 2 deep, high in the sky.
  auto pond = [](World &world, BiomeType b) {
    auto c = findBiomeChunk(b);
    REQUIRE(c);
    world.loadChunkSync(c->x, c->y);
    glm::ivec3 cell{c->x * kChunkWidth + 5, 180, c->y * kChunkDepth + 5};
    fill(world, cell - glm::ivec3(0, 1, 0), cell, BlockType::WATER);
    return cell;
  };
  World world;
  glm::ivec3 ocean = pond(world, BiomeType::OCEAN);
  glm::ivec3 plains = pond(world, BiomeType::PLAINS);
  glm::ivec3 desert = pond(world, BiomeType::DESERT);

  CHECK(fishCanSpawn(EntityKind::Sunfin, world, ocean));
  CHECK_FALSE(fishCanSpawn(EntityKind::Sunfin, world, plains));
  CHECK(fishCanSpawn(EntityKind::Pebblefish, world, plains));
  CHECK(fishCanSpawn(EntityKind::Pebblefish, world, desert));
  CHECK_FALSE(fishCanSpawn(EntityKind::Pebblefish, world, ocean));
  // Open sky: no Glowfin.
  for (glm::ivec3 c : {ocean, plains, desert}) CHECK_FALSE(fishCanSpawn(EntityKind::Glowfin, world, c));

  // Put a lid over the plains pond (with an air gap, like a cave): Glowfin
  // only, and not Pebblefish.
  fill(world, plains + glm::ivec3(-3, -2, -3), plains + glm::ivec3(3, 2, 3), BlockType::STONE);
  fill(world, plains + glm::ivec3(-2, -1, -2), plains + glm::ivec3(2, 1, 2), BlockType::AIR);
  fill(world, plains - glm::ivec3(0, 1, 0), plains, BlockType::WATER);
  CHECK(fishCanSpawn(EntityKind::Glowfin, world, plains));
  CHECK_FALSE(fishCanSpawn(EntityKind::Pebblefish, world, plains));

  // Too shallow, or not water at all.
  fill(world, ocean - glm::ivec3(0, 1, 0), ocean - glm::ivec3(0, 1, 0), BlockType::SAND);
  CHECK_FALSE(fishCanSpawn(EntityKind::Sunfin, world, ocean));
  CHECK_FALSE(fishCanSpawn(EntityKind::Sunfin, world, ocean + glm::ivec3(0, 1, 0)));
  CHECK_FALSE(fishCanSpawn(EntityKind::Woolly, world, ocean));
}

TEST_CASE("on generated terrain, fish appear only in water where they live") {
  auto sea = findBiomeChunk(BiomeType::OCEAN);
  REQUIRE(sea);
  World world;
  for (int cx = sea->x - 5; cx <= sea->x + 5; ++cx)
    for (int cz = sea->y - 5; cz <= sea->y + 5; ++cz) world.loadChunkSync(cx, cz);
  EntityStore store;
  Spawner spawner;
  registerFishSpawns(spawner);
  Rng rng{2024};
  glm::vec3 player{sea->x * kChunkWidth + 8.0f, kSeaLevel + 1.0f, sea->y * kChunkDepth + 8.0f};
  std::vector<glm::vec3> players{player};
  for (int i = 0; i < 20 * 60; ++i) spawner.tick(store, world, rng, players, 0.0f);

  REQUIRE(store.size() > 0); // the sea has fish, by night too
  for (const auto &[id, e] : store.all()) {
    REQUIRE(isFish(e.kind));
    CAPTURE(entityType(e.kind).name);
    CHECK(SwimBrain::waterAhead(e, world, glm::vec3(0.0f), 0.0f));
    const FishDef &f = *entityType(e.kind).fish;
    if (f.cave) continue;
    int cx = static_cast<int>(std::floor(e.body.position.x / kChunkWidth));
    int cz = static_cast<int>(std::floor(e.body.position.z / kChunkDepth));
    BiomeType b = BiomeManager::getBiomeForChunk(cx, cz);
    CHECK(std::find(f.biomes.begin(), f.biomes.end(), b) != f.biomes.end());
  }
}
