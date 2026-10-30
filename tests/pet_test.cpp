// Pets: strays tamed with treats, then following, sitting and saved.
#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "entity/animals.hpp"
#include "entity/entity_store.hpp"
#include "item/recipes.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include "world/world_save.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {

constexpr int kBase = 200; // clear sky above the rig

std::vector<Event> pollAll(LocalConnection &conn) {
  std::vector<Event> out;
  conn.poll(out);
  return out;
}

bool rejected(const std::vector<Event> &evs) {
  return std::any_of(evs.begin(), evs.end(), [](const Event &e) {
    return std::holds_alternative<CommandRejected>(e);
  });
}

template <class T> const T *find(const std::vector<Event> &evs) {
  for (const Event &e : evs)
    if (auto *t = std::get_if<T>(&e)) return t;
  return nullptr;
}

// A stone field in the sky over chunks x 0..2, z 0: x 0..47, z 0..15, its
// top at kBase. The host stands near the west end; a stray puppy is in front.
struct PetGame {
  // Radius 1: the field's chunks stay loaded while the owner walks across it.
  ScopedRenderDistance rd{1};
  Game game;
  LocalConnection host, guest;
  PlayerId hid = 0, gid = 0;
  EntityId pup = 0;
  static inline const glm::vec3 kStand{4.5f, kBase + 1.0f, 8.5f};

  PetGame() {
    game.spawner() = Spawner{};
    hid = join(host, "", Game::Role::Host);
    build();
    stand(host, hid, kStand);
    auto p = game.entities().spawn(EntityKind::Puppy, kStand + glm::vec3(0, 0, -2));
    REQUIRE(p);
    pup = *p;
  }

  void build() {
    for (int cx = 0; cx < 3; ++cx) game.world().loadChunkSync(cx, 0);
    fill(game.world(), {0, kBase - 2, 0}, {47, kBase, 15}, BlockType::STONE);
    fill(game.world(), {0, kBase + 1, 0}, {47, kBase + 12, 15}, BlockType::AIR);
  }

  PlayerId join(LocalConnection &conn, std::string name, Game::Role role) {
    game.addClient(conn, role);
    conn.send(Join{std::move(name)});
    game.pump();
    auto evs = pollAll(conn);
    auto joined = find<Joined>(evs);
    REQUIRE(joined);
    return joined->id;
  }
  PlayerId joinGuest(std::string name = "Sam") {
    gid = join(guest, std::move(name), Game::Role::Guest);
    stand(guest, gid, kStand + glm::vec3(0, 0, 3));
    return gid;
  }

  PlayerState &ps(PlayerId id) { return *game.findPlayer(id); }
  Entity *pet() { return game.entities().find(pup); }

  std::vector<Event> run(LocalConnection &conn, Command cmd) {
    conn.send(std::move(cmd));
    game.pump();
    return pollAll(conn);
  }
  void stand(LocalConnection &conn, PlayerId id, glm::vec3 at) {
    MovePlayer m;
    m.position = at;
    m.respawn = ps(id).respawn;
    run(conn, m);
  }
  void hold(PlayerId id, ItemId item, uint16_t n = 1) {
    ps(id).inventory.set(ps(id).selected_slot, n ? ItemStack{item, n} : ItemStack{});
  }
  int held(PlayerId id) { return ps(id).inventory.slot(ps(id).selected_slot).count; }
  void wait(float seconds) {
    game.advance(seconds);
    pollAll(host);
    pollAll(guest);
  }
  // Keeps the pet where it was put while timers run.
  void waitHere(float seconds) {
    glm::vec3 at = pet()->body.position;
    wait(seconds);
    pet()->body.position = at;
    pet()->body.velocity = glm::vec3(0.0f);
  }

  // Three treats, a second apart: tamed by `who`.
  std::vector<Event> tame(LocalConnection &conn, PlayerId who) {
    hold(who, ItemId::BERRIES, 3);
    std::vector<Event> last;
    for (int i = 0; i < 3; ++i) {
      if (i) waitHere(1.05f);
      last = run(conn, InteractEntity{pup});
      REQUIRE_FALSE(rejected(last));
    }
    return last;
  }

  float distanceTo(PlayerId id) {
    glm::vec3 a = pet()->body.position, b = ps(id).position;
    return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
  }
};

} // namespace

// ─── Types ─────────────────────────────────────────────────────────────

TEST_CASE("puppies and kittens are pinned, friendly pets with one collar") {
  CHECK(static_cast<int>(EntityKind::Puppy) == 17);
  CHECK(static_cast<int>(EntityKind::Kitten) == 18);
  for (EntityKind k : kPetKinds) {
    CAPTURE(static_cast<int>(k));
    const EntityTypeDef &def = entityType(k);
    CHECK(isAnimal(k));
    CHECK(isPetKind(k));
    CHECK(def.max_health == 0); // can't be hurt
    CHECK(def.animal->tame_treats == 3);
    CHECK(def.animal->product.empty());
    CHECK(def.sit_drop > 0.0f);
    CHECK(def.brain);
    CHECK(std::count_if(def.model.begin(), def.model.end(),
                        [](const ModelBox &b) { return b.collar; }) == 1);
  }
  for (EntityKind k : kAnimalKinds) CHECK_FALSE(isPetKind(k));
  CHECK(collarColour(1) != collarColour(2));
}

TEST_CASE("every treat can be had in survival") {
  // Berries from Bunnies, raw fish from a rod, bread crafted from farmed wheat
  // and strawberries grown from seed.
  auto fromAnimal = [](ItemId id) {
    for (EntityKind k : kAnimalKinds)
      if (entityType(k).animal->product.id == id) return true;
    return false;
  };
  auto fromFish = [](ItemId id) {
    for (EntityKind k : kFishKinds)
      if (entityType(k).fish->raw == id) return true;
    return false;
  };
  auto crafted = [](ItemId id) {
    for (const Recipe &r : recipes())
      if (r.makes.id == id) return true;
    return false;
  };
  for (EntityKind k : kPetKinds)
    for (ItemId treat : entityType(k).animal->eats) {
      CAPTURE(static_cast<int>(treat));
      CHECK((fromAnimal(treat) || fromFish(treat) || crafted(treat) ||
             treat == ItemId::STRAWBERRY));
    }
}

TEST_CASE("strays spawn in their biomes by day, one of each kind per player") {
  Spawner s;
  registerAnimalSpawns(s);
  int pets = 0;
  for (const SpawnRule &r : s.rules())
    if (isPetKind(r.kind)) {
      ++pets;
      CHECK(r.max_per_player == kStraysPerPlayer);
      CHECK(r.interval_ms == kStraySpawnIntervalMs);
    }
  CHECK(pets == static_cast<int>(std::size(kPetKinds)));
}

// ─── Brain, despawning and caps ────────────────────────────────────────

TEST_CASE("a tamed pet is never despawned, a stray is") {
  ScopedRenderDistance rd{0};
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {0, kBase - 1, 0}, {15, kBase, 15}, BlockType::STONE);
  EntityStore store;
  Rng rng{3};
  auto stray = store.spawn(EntityKind::Puppy, {4.5f, kBase + 1.0f, 4.5f});
  auto tamed = store.spawn(EntityKind::Kitten, {8.5f, kBase + 1.0f, 8.5f});
  REQUIRE(stray);
  REQUIRE(tamed);
  store.find(*tamed)->pet = PetOwner{};
  std::vector<glm::vec3> far{{200.0f, kBase, 200.0f}};
  store.tick(world, rng, far);
  CHECK_FALSE(store.find(*stray));
  CHECK(store.find(*tamed));
}

TEST_CASE("a tamed pet doesn't stop a stray of its kind turning up") {
  ScopedRenderDistance rd{0};
  World world;
  for (int cx = -3; cx <= 3; ++cx)
    for (int cz = -3; cz <= 3; ++cz) world.loadChunkSync(cx, cz);
  EntityStore store;
  Rng rng{9};
  const glm::vec3 player{8.0f, 100.0f, 8.0f};
  auto tamed = store.spawn(EntityKind::Puppy, player);
  REQUIRE(tamed);
  store.find(*tamed)->pet = PetOwner{};
  Spawner s;
  s.addRule({EntityKind::Puppy, 1, 50,
             [](const World &, glm::ivec3, float) { return true; }});
  std::vector<glm::vec3> players{player};
  for (int i = 0; i < 100 && store.size() < 2; ++i) s.tick(store, world, rng, players, 0.5f);
  CHECK(store.size() == 2);
}

// ─── Taming ────────────────────────────────────────────────────────────

TEST_CASE_FIXTURE(PetGame, "three treats tame a stray, a heart each time") {
  hold(hid, ItemId::BERRIES, 5);
  CHECK_FALSE(rejected(run(host, InteractEntity{pup})));
  CHECK(held(hid) == 4);
  CHECK(pet()->happy_ms > 0);
  CHECK(pet()->treats == 1);
  CHECK_FALSE(pet()->pet);

  waitHere(1.05f);
  run(host, InteractEntity{pup});
  CHECK_FALSE(pet()->pet);
  waitHere(1.05f);
  auto evs = run(host, InteractEntity{pup});
  CHECK(held(hid) == 2);
  REQUIRE(pet()->pet);
  CHECK(pet()->pet->host);
  CHECK(pet()->pet->collar >= 1);
  CHECK(pet()->pet->collar <= kCollarColours);
  CHECK_FALSE(pet()->pet->sitting);
  auto note = find<Notice>(evs);
  REQUIRE(note);
  CHECK(note->text == "The Puppy is your pet now!");
}

TEST_CASE_FIXTURE(PetGame, "a stray refuses the wrong food, and treats too quickly") {
  hold(hid, ItemId::RAW_SUNFIN, 3); // a kitten's treat
  CHECK(rejected(run(host, InteractEntity{pup})));
  CHECK(held(hid) == 3);
  CHECK(pet()->treats == 0);

  hold(hid, ItemId::BREAD, 3);
  CHECK_FALSE(rejected(run(host, InteractEntity{pup})));
  CHECK(rejected(run(host, InteractEntity{pup}))); // the same click, held
  CHECK(held(hid) == 2);
  CHECK(pet()->treats == 1);

  hold(hid, ItemId::NONE, 0);
  waitHere(1.05f);
  CHECK(rejected(run(host, InteractEntity{pup})));
}

TEST_CASE_FIXTURE(PetGame, "taming needs reach") {
  hold(hid, ItemId::BERRIES, 3);
  pet()->body.position = kStand + glm::vec3(0, 0, -10);
  CHECK(rejected(run(host, InteractEntity{pup})));
  CHECK(held(hid) == 3);
}

TEST_CASE_FIXTURE(PetGame, "six pets is the most: the seventh's last treat is refused") {
  for (int i = 0; i < Game::kMaxPets; ++i) {
    auto id = game.entities().spawn(EntityKind::Kitten, kStand + glm::vec3(2, 0, 0));
    REQUIRE(id);
    game.entities().find(*id)->pet = PetOwner{true, "", 1, true};
  }
  hold(hid, ItemId::BERRIES, 3);
  run(host, InteractEntity{pup});
  waitHere(1.05f);
  run(host, InteractEntity{pup});
  waitHere(1.05f);
  auto evs = run(host, InteractEntity{pup});
  CHECK(rejected(evs));
  CHECK(held(hid) == 1);
  CHECK_FALSE(pet()->pet);
}

// ─── Owning a pet ──────────────────────────────────────────────────────

TEST_CASE_FIXTURE(PetGame, "the owner's click sits a pet down and stands it up; a treat doesn't") {
  tame(host, hid);
  hold(hid, itemForBlock(BlockType::STONE), 4);
  run(host, InteractEntity{pup});
  CHECK(pet()->pet->sitting);
  CHECK(held(hid) == 4); // nothing used
  hold(hid, ItemId::NONE, 0);
  run(host, InteractEntity{pup});
  CHECK_FALSE(pet()->pet->sitting);

  hold(hid, ItemId::BREAD, 2);
  waitHere(2.1f);
  CHECK_FALSE(rejected(run(host, InteractEntity{pup})));
  CHECK(held(hid) == 1);
  CHECK(pet()->happy_ms > 0);
  CHECK_FALSE(pet()->pet->sitting);
}

TEST_CASE_FIXTURE(PetGame, "someone else's pet gives a heart but won't sit for you") {
  tame(host, hid);
  joinGuest();
  hold(gid, ItemId::NONE, 0);
  auto evs = run(guest, InteractEntity{pup});
  CHECK(rejected(evs));
  CHECK(pet()->happy_ms > 0);
  CHECK_FALSE(pet()->pet->sitting);
}

TEST_CASE_FIXTURE(PetGame, "a guest can tame a pet of their own") {
  joinGuest("Sam");
  stand(guest, gid, kStand + glm::vec3(0, 0, -1));
  tame(guest, gid);
  REQUIRE(pet()->pet);
  CHECK_FALSE(pet()->pet->host);
  CHECK(pet()->pet->name == "Sam");
  hold(hid, ItemId::NONE, 0);
  CHECK(rejected(run(host, InteractEntity{pup})));
}

TEST_CASE_FIXTURE(PetGame, "a pet walks after its owner and stops close by") {
  tame(host, hid);
  stand(host, hid, {30.5f, kBase + 1.0f, 8.5f}); // 26 away would catch up; walk 15
  stand(host, hid, {19.5f, kBase + 1.0f, 8.5f});
  wait(8.0f);
  CAPTURE(pet()->body.position.x);
  CAPTURE(pet()->body.position.y);
  CHECK(distanceTo(hid) <= PetBrain::kFollowFrom + 0.5f);
  CHECK(distanceTo(hid) >= 1.0f); // not on top of them
}

TEST_CASE_FIXTURE(PetGame, "a sitting pet stays put") {
  tame(host, hid);
  hold(hid, ItemId::NONE, 0);
  run(host, InteractEntity{pup});
  REQUIRE(pet()->pet->sitting);
  glm::vec3 at = pet()->body.position;
  stand(host, hid, {16.5f, kBase + 1.0f, 8.5f});
  wait(10.0f);
  CHECK(glm::distance(pet()->body.position, at) < 0.1f);
}

TEST_CASE_FIXTURE(PetGame, "a pet left far behind lands beside its owner") {
  tame(host, hid);
  stand(host, hid, {40.5f, kBase + 1.0f, 8.5f});
  wait(1.1f);
  CHECK(distanceTo(hid) <= Game::kCatchUpRange + 1.0f);
  CHECK(pet()->body.position.y == doctest::Approx(kBase + 1.0f).epsilon(0.01));
}

TEST_CASE_FIXTURE(PetGame, "a pet boxed in is brought along after a few seconds") {
  tame(host, hid);
  // Walls all round the pet, too high to hop.
  glm::ivec3 c = glm::ivec3(glm::floor(pet()->body.position));
  fill(game.world(), c + glm::ivec3(-1, 0, -1), c + glm::ivec3(1, 2, 1), BlockType::STONE);
  fill(game.world(), c, c + glm::ivec3(0, 1, 0), BlockType::AIR);
  stand(host, hid, {14.5f, kBase + 1.0f, 8.5f});
  wait(3.0f);
  CHECK(distanceTo(hid) > 5.0f); // still stuck
  wait(4.0f);
  CHECK(distanceTo(hid) <= PetBrain::kFollowFrom + 1.0f);
}

TEST_CASE_FIXTURE(PetGame, "a pet whose owner is away waits where it is") {
  joinGuest("Sam");
  stand(guest, gid, kStand + glm::vec3(0, 0, -1));
  tame(guest, gid);
  game.removeClient(guest);
  glm::vec3 at = pet()->body.position;
  wait(10.0f);
  CHECK(glm::length(glm::vec2(pet()->body.position.x - at.x, pet()->body.position.z - at.z)) <
        0.1f);
  CHECK_FALSE(pet()->follow);
}
