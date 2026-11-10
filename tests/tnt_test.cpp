// TNT in the Game: lighting it with a torch, the fuse, the explosion's crater,
// drops, hurting and shoving, chain reactions and saving with one lit.
#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "block/door.hpp"
#include "entity/entity_store.hpp"
#include "item/mining.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include "world/world_save.hpp"

namespace {

constexpr int kBase = 200; // the range's ground: clear sky above

std::vector<Event> pollAll(LocalConnection &conn) {
  std::vector<Event> out;
  conn.poll(out);
  return out;
}

template <class T> std::optional<T> find(const std::vector<Event> &evs) {
  for (const Event &e : evs)
    if (const T *t = std::get_if<T>(&e)) return *t;
  return std::nullopt;
}

template <class T> int countOf(const std::vector<Event> &evs) {
  int n = 0;
  for (const Event &e : evs) n += std::holds_alternative<T>(e);
  return n;
}

// A slab of stone 12 deep over chunks (-1..1, -1..1), clear sky above, with
// one player standing on it.
struct Range {
  ScopedRenderDistance rd{0};
  Game game;
  LocalConnection a;
  PlayerId id = 0;
  static inline const glm::vec3 kStand{2.5f, kBase + 1.0f, 2.5f};

  explicit Range(GameMode mode = GameMode::Survival) {
    game.spawner() = Spawner{};
    game.setMode(mode);
    game.addClient(a);
    a.send(Join{});
    game.pump();
    auto joined = find<Joined>(pollAll(a));
    REQUIRE(joined);
    id = joined->id;
    for (int x = -1; x <= 1; ++x)
      for (int z = -1; z <= 1; ++z) game.world().loadChunkSync(x, z);
    fill(game.world(), {-16, kBase - 12, -16}, {31, kBase, 31}, BlockType::STONE);
    fill(game.world(), {-16, kBase + 1, -16}, {31, kBase + 40, 31}, BlockType::AIR);
    stand(kStand);
  }

  PlayerState &ps() { return *game.findPlayer(id); }
  std::vector<Event> run(Command cmd) {
    a.send(std::move(cmd));
    game.pump();
    return pollAll(a);
  }
  void stand(glm::vec3 at) {
    MovePlayer m;
    m.position = at;
    m.yaw = -90.0f;
    m.on_ground = true;
    m.respawn = ps().respawn;
    run(m);
    ps().position = at;
  }
  void holdItem(ItemId item, uint16_t n = 1) { ps().inventory.set(ps().selected_slot, {item, n}); }
  void holdTorch(uint16_t n = 1) { holdItem(itemForBlock(BlockType::TORCH), n); }
  BlockType at(glm::ivec3 p) const { return game.world().getBlockAt(glm::vec3(p) + 0.5f); }
  void set(glm::ivec3 p, BlockType b) { game.world().setBlockAt(p, b); }
  // Lights the TNT at `p` from right beside it.
  std::vector<Event> light(glm::ivec3 p) {
    holdTorch();
    ps().position = glm::vec3(p) + glm::vec3(0.5f, 0.0f, 2.5f);
    return run(UseItemOn{p});
  }
  std::vector<Event> ticks(int n) {
    for (int i = 0; i < n; ++i) game.tick();
    return pollAll(a);
  }
  int litTnt() const {
    int n = 0;
    for (const auto &[eid, e] : game.entities().all()) n += e.kind == EntityKind::LitTnt;
    return n;
  }
  int carried(ItemId item) {
    int n = 0;
    for (const ItemStack &s : ps().inventory.slots()) n += s.id == item ? s.count : 0;
    return n;
  }
  int dropped(ItemId item) const {
    int n = 0;
    for (const auto &[eid, e] : game.entities().all())
      if (e.kind == EntityKind::DroppedItem && e.item.id == item) n += e.item.count;
    return n;
  }
};

const glm::ivec3 kSpot{4, kBase + 1, 4}; // on the ground, in reach of the player
constexpr int kFuseTicks = Game::kTntFuseMs / Game::kTickMs;

} // namespace

// ─── Lighting ──────────────────────────────────────────────────────────

TEST_CASE("a torch lights TNT: the block becomes one lit TNT, and the torch stays") {
  Range r;
  r.set(kSpot, BlockType::TNT);
  auto evs = r.light(kSpot);
  CHECK(r.at(kSpot) == BlockType::AIR);
  CHECK(r.litTnt() == 1);
  CHECK(r.ps().inventory.slot(r.ps().selected_slot).count == 1);
  auto cue = find<SoundCue>(evs);
  REQUIRE(cue);
  CHECK(cue->cue == Cue::TntFuse);
  // It hops as it's lit.
  const Entity &tnt = r.game.entities().all().begin()->second;
  CHECK(tnt.body.velocity.y > 0.0f);
  CHECK(tnt.fuse_ms == Game::kTntFuseMs);
}

TEST_CASE("anything but a torch doesn't light TNT, and breaking it gives it back") {
  Range r;
  r.set(kSpot, BlockType::TNT);
  r.holdItem(ItemId::COAL);
  auto evs = r.run(UseItemOn{kSpot});
  CHECK(find<CommandRejected>(evs));
  r.holdItem(ItemId::NONE);
  r.run(UseItemOn{kSpot});
  CHECK(r.at(kSpot) == BlockType::TNT);
  CHECK(r.litTnt() == 0);

  r.run(StartBreak{kSpot});
  r.run(BreakBlock{kSpot}); // picked up at once
  CHECK(r.at(kSpot) == BlockType::AIR);
  CHECK(r.litTnt() == 0);
  CHECK(r.carried(itemForBlock(BlockType::TNT)) == 1);
}

TEST_CASE("TNT lights in creative too") {
  Range r(GameMode::Creative);
  r.set(kSpot, BlockType::TNT);
  r.light(kSpot);
  CHECK(r.litTnt() == 1);
}

TEST_CASE("lit TNT falls and comes to rest") {
  Range r;
  const glm::ivec3 high = kSpot + glm::ivec3(0, 3, 0);
  r.set(high, BlockType::TNT);
  r.light(high);
  r.ticks(40);
  REQUIRE(r.litTnt() == 1);
  const Entity &tnt = r.game.entities().all().begin()->second;
  CHECK(tnt.body.on_ground);
  CHECK(tnt.body.position.y == doctest::Approx(kBase + 1.0f).epsilon(0.01));
}

TEST_CASE("the fuse burns four seconds, then it's gone") {
  Range r;
  r.set(kSpot, BlockType::TNT);
  r.light(kSpot);
  r.ticks(kFuseTicks - 1);
  CHECK(r.litTnt() == 1);
  r.ticks(1);
  CHECK(r.litTnt() == 0);
}

TEST_CASE("a lit TNT is never tidied away, however far off") {
  Range r;
  r.set(kSpot, BlockType::TNT);
  r.light(kSpot);
  r.ps().position = glm::vec3(kSpot) + glm::vec3(EntityStore::kDespawnDistance + 50.0f, 0, 0);
  r.ticks(5);
  CHECK(r.litTnt() == 1);
}

// ─── Saving ────────────────────────────────────────────────────────────

TEST_CASE("a world saved with a lit TNT has a TNT block there, unlit") {
  ScopedSeed seed;
  TempDir dir;
  {
    Range r;
    r.game.load(dir.path.string());
    for (int x = -1; x <= 1; ++x)
      for (int z = -1; z <= 1; ++z) r.game.world().loadChunkSync(x, z);
    fill(r.game.world(), {0, kBase - 2, 0}, {15, kBase, 15}, BlockType::STONE);
    fill(r.game.world(), {0, kBase + 1, 0}, {15, kBase + 10, 15}, BlockType::AIR);
    r.set(kSpot, BlockType::TNT);
    r.light(kSpot);
    r.ticks(30); // landed again, still burning
    REQUIRE(r.litTnt() == 1);
    REQUIRE(r.game.save());
    // Saving doesn't put it out.
    CHECK(r.litTnt() == 1);
    CHECK(r.at(kSpot) == BlockType::AIR);
  }
  WorldEdits edits;
  REQUIRE(WorldSave::loadEdits(dir.path.string(), edits));
  const uint32_t idx = kSpot.x + kChunkWidth * (kSpot.z + kChunkDepth * kSpot.y);
  REQUIRE(edits.count({0, 0}));
  CHECK(edits[{0, 0}][idx] == static_cast<uint8_t>(BlockType::TNT));
}

// ─── The explosion ─────────────────────────────────────────────────────

namespace {

// Lights TNT buried at `p` and waits for it to go off; the player stands
// well away. Returns what the player was sent meanwhile.
std::vector<Event> blastAt(Range &r, glm::ivec3 p) {
  r.set(p, BlockType::TNT);
  r.light(p);
  r.ps().position = glm::vec3(-14.5f, kBase + 1.0f, -14.5f);
  return r.ticks(kFuseTicks);
}

const glm::ivec3 kBuried{8, kBase - 5, 8}; // in the stone, far from its edges

} // namespace

TEST_CASE("the crater: everything within 3 is gone, nothing past 4 is touched") {
  Range r;
  r.game.rng().seed(7);
  auto evs = blastAt(r, kBuried);
  REQUIRE(r.litTnt() == 0);
  const glm::vec3 centre = glm::vec3(kBuried) + 0.5f;
  int broken = 0;
  for (int dy = -6; dy <= 5; ++dy) // the ground's top is 5 above it
    for (int dz = -6; dz <= 6; ++dz)
      for (int dx = -6; dx <= 6; ++dx) {
        const glm::ivec3 p = kBuried + glm::ivec3(dx, dy, dz);
        const float d = glm::distance(glm::vec3(p) + 0.5f, centre);
        CAPTURE(p);
        if (d <= Game::kBlastRadius) CHECK(r.at(p) == BlockType::AIR);
        if (d > Game::kBlastEdge) CHECK(r.at(p) == BlockType::STONE);
        broken += r.at(p) == BlockType::AIR;
      }
  CHECK(broken >= 100);
  CHECK(broken <= 270);
  // One batch for the crater, and the boom where it was.
  CHECK(countOf<BlocksChanged>(evs) == 1);
  auto boom = find<Exploded>(evs);
  REQUIRE(boom);
  CHECK(glm::distance(boom->position, centre) < 0.6f);
}

TEST_CASE("bedrock, water and a treasure chest hold") {
  Range r;
  const glm::ivec3 bedrock = kBuried + glm::ivec3(1, 0, 0);
  const glm::ivec3 treasure = kBuried + glm::ivec3(0, 0, 1);
  const glm::ivec3 water = kBuried + glm::ivec3(-1, 0, 0);
  r.set(bedrock, BlockType::BEDROCK);
  r.set(treasure, BlockType::TREASURE_CHEST);
  r.set(water, BlockType::WATER);
  blastAt(r, kBuried);
  CHECK(r.at(bedrock) == BlockType::BEDROCK);
  CHECK(r.at(treasure) == BlockType::TREASURE_CHEST);
  CHECK(r.at(water) == BlockType::WATER);
}

TEST_CASE("TNT that goes off in water breaks no blocks, but still hurts") {
  Range r;
  // A flooded pit: lit TNT sinks to its floor.
  fill(r.game.world(), {6, kBase - 3, 6}, {10, kBase, 10}, BlockType::WATER);
  const glm::ivec3 floor_tnt{8, kBase - 3, 8};
  r.set(floor_tnt, BlockType::TNT);
  r.light(floor_tnt);
  r.ps().position = glm::vec3(8.5f, kBase + 1.0f, 8.5f); // on the bank, close
  r.ps().vitals.reset();
  auto evs = r.ticks(kFuseTicks);
  CHECK(countOf<BlocksChanged>(evs) == 0);
  CHECK(find<Exploded>(evs));
  CHECK(r.at(floor_tnt - glm::ivec3(0, 1, 0)) == BlockType::STONE);
  CHECK(r.at({5, kBase - 2, 8}) == BlockType::STONE);
  CHECK(r.ps().vitals.health() < Vitals::kMax);
}

TEST_CASE("survival: every broken block drops, ore without a pickaxe too") {
  Range r;
  r.set(kBuried + glm::ivec3(1, 0, 0), BlockType::DIAMOND_ORE);
  r.set(kBuried + glm::ivec3(0, 1, 0), BlockType::COAL_ORE);
  blastAt(r, kBuried);
  CHECK(r.dropped(ItemId::DIAMOND) == 1);
  CHECK(r.dropped(ItemId::COAL) == 1);
  // Stone drops cobblestone, a stack of each kind rather than one apiece.
  const int cobble = r.dropped(itemForBlock(BlockType::COBBLESTONE));
  CHECK(cobble >= 90);
  int stacks = 0;
  for (const auto &[eid, e] : r.game.entities().all())
    stacks += e.kind == EntityKind::DroppedItem;
  CHECK(stacks <= 6);
}

TEST_CASE("creative: nothing drops") {
  Range r(GameMode::Creative);
  r.set(kBuried + glm::ivec3(1, 0, 0), BlockType::DIAMOND_ORE);
  blastAt(r, kBuried);
  CHECK(r.at(kBuried + glm::ivec3(1, 0, 0)) == BlockType::AIR);
  CHECK(r.dropped(ItemId::DIAMOND) == 0);
  CHECK(r.dropped(itemForBlock(BlockType::COBBLESTONE)) == 0);
}

TEST_CASE("a door goes as a whole and drops one; a chest spills and drops itself") {
  Range r;
  // A door standing on the ground beside the TNT, its top out of the crater's core.
  const glm::ivec3 tnt{8, kBase + 1, 8};
  const glm::ivec3 door{10, kBase + 1, 8};
  r.set(door, doorBlock(false, DoorFacing::North, false));
  r.set(door + glm::ivec3(0, 1, 0), doorBlock(true, DoorFacing::North, false));
  const glm::ivec3 chest{8, kBase, 10};
  r.set(chest, BlockType::CHEST);
  REQUIRE(r.game.chestAt(chest) == nullptr); // set straight in: give it contents
  r.holdItem(itemForBlock(BlockType::CHEST));
  r.set(chest, BlockType::AIR);
  r.ps().position = glm::vec3(8.5f, kBase + 1.0f, 12.5f);
  r.run(PlaceBlock{chest, {0, -1, 0}});
  REQUIRE(r.at(chest) == BlockType::CHEST);
  REQUIRE(r.game.chestAt(chest));
  (*r.game.chestAt(chest))[0] = {ItemId::RUBY, 3};

  blastAt(r, tnt);
  CHECK(r.at(door) == BlockType::AIR);
  CHECK(r.at(door + glm::ivec3(0, 1, 0)) == BlockType::AIR);
  CHECK(r.dropped(ItemId::WOODEN_DOOR) == 1);
  CHECK(r.at(chest) == BlockType::AIR);
  CHECK(r.dropped(ItemId::RUBY) == 3);
  CHECK(r.dropped(itemForBlock(BlockType::CHEST)) == 1);
}

TEST_CASE("TNT caught in a blast is lit, and goes off a moment later") {
  Range r;
  const glm::ivec3 next = kBuried + glm::ivec3(2, 0, 0);
  r.set(next, BlockType::TNT);
  blastAt(r, kBuried);
  REQUIRE(r.litTnt() == 1);
  const Entity &lit = r.game.entities().all().rbegin()->second;
  CHECK(lit.kind == EntityKind::LitTnt);
  CHECK(lit.fuse_ms >= Game::kChainFuseMinMs);
  CHECK(lit.fuse_ms <= Game::kChainFuseMaxMs);
  auto evs = r.ticks(Game::kChainFuseMaxMs / Game::kTickMs);
  CHECK(r.litTnt() == 0);
  CHECK(find<Exploded>(evs));
  CHECK(r.at(next + glm::ivec3(3, 0, 0)) == BlockType::AIR); // its own crater, further on
}

TEST_CASE("at most four go off a tick; the rest wait their turn") {
  Range r;
  // All in one chunk: with no render distance, the world keeps only the
  // player's chunk loaded as they move about.
  for (int i = 0; i < 10; ++i) {
    const glm::ivec3 p{1 + 3 * (i % 5), kBase + 1, 2 + 6 * (i / 5)};
    r.set(p, BlockType::TNT);
    auto evs = r.light(p);
    CAPTURE(i);
    CHECK_FALSE(find<CommandRejected>(evs));
  }
  REQUIRE(r.litTnt() == 10);
  r.ps().position = glm::vec3(8.5f, kBase + 30.0f, 8.5f); // flying well above
  r.ticks(kFuseTicks - 1);
  auto count = [&](const std::vector<Event> &evs) { return countOf<Exploded>(evs); };
  // Lit a pump apart, they all burn out on the same tick.
  CHECK(count(r.ticks(1)) == 4);
  CHECK(r.game.pendingBlasts() == 6);
  CHECK(count(r.ticks(1)) == 4);
  CHECK(count(r.ticks(1)) == 2);
  CHECK(r.game.pendingBlasts() == 0);
}

// ─── Hurting and shoving ───────────────────────────────────────────────

TEST_CASE("close by, it hurts 12 and throws you; from full health you live") {
  Range r;
  const glm::ivec3 tnt{8, kBase + 1, 8};
  r.set(tnt, BlockType::TNT);
  r.light(tnt);
  r.ticks(kFuseTicks - 1);
  // Right on top of it as it goes.
  r.ps().position = glm::vec3(8.5f, kBase + 1.0f - kPlayerHeight / 2 + 0.49f, 8.5f);
  r.ps().vitals.reset();
  auto evs = r.ticks(1);
  CHECK(r.ps().vitals.health() == Vitals::kMax - Game::kBlastDamage);
  CHECK_FALSE(r.ps().vitals.dead());
  auto knocked = find<KnockedBack>(evs);
  REQUIRE(knocked);
  CHECK(knocked->velocity.y > 0.0f);
}

TEST_CASE("past seven blocks it does nothing; nearer, less the further") {
  Range r;
  auto hurtAt = [&](float dist) {
    const glm::ivec3 tnt{8, kBase + 1, 8};
    r.set(tnt, BlockType::TNT);
    r.light(tnt);
    r.ticks(kFuseTicks - 1);
    // Where it is now (each lands in the last one's crater).
    const Entity &lit = r.game.entities().all().rbegin()->second;
    REQUIRE(lit.kind == EntityKind::LitTnt);
    const glm::vec3 centre = lit.body.position + glm::vec3(0.0f, lit.body.height / 2, 0.0f);
    r.ps().position = centre + glm::vec3(dist, -kPlayerHeight / 2, 0.0f);
    r.ps().vitals.reset();
    auto evs = r.ticks(1);
    return std::pair{Vitals::kMax - r.ps().vitals.health(), find<KnockedBack>(evs).has_value()};
  };
  auto far = hurtAt(Game::kBlastReach + 0.5f);
  CHECK(far.first == 0);
  CHECK_FALSE(far.second);
  const int mid = hurtAt(3.5f).first;
  const int near = hurtAt(1.0f).first;
  CHECK(mid > 0);
  CHECK(near > mid);
}

TEST_CASE("two at once can kill, and the death line says TNT") {
  Range r;
  for (glm::ivec3 tnt : {glm::ivec3{8, kBase + 1, 8}, glm::ivec3{9, kBase + 1, 8}}) {
    r.set(tnt, BlockType::TNT);
    r.light(tnt);
  }
  r.ticks(kFuseTicks - 1);
  r.ps().position = glm::vec3(9.0f, kBase + 1.0f, 8.5f);
  r.ps().vitals.reset();
  auto evs = r.ticks(1);
  CHECK(r.ps().vitals.dead());
  bool told = false;
  for (const Event &e : evs)
    if (const auto *m = std::get_if<ChatMessage>(&e))
      told |= m->text.find("was blown up by TNT") != std::string::npos;
  CHECK(told);
}

TEST_CASE("creative players are thrown but not hurt") {
  Range r(GameMode::Creative);
  const glm::ivec3 tnt{8, kBase + 1, 8};
  r.set(tnt, BlockType::TNT);
  r.light(tnt);
  r.ticks(kFuseTicks - 1);
  r.ps().position = glm::vec3(9.5f, kBase + 1.0f, 8.5f);
  auto evs = r.ticks(1);
  CHECK(r.ps().vitals.health() == Vitals::kMax);
  CHECK(find<KnockedBack>(evs));
}

TEST_CASE("creatures that can be hit are hurt; pets and animals are only thrown") {
  Range r;
  const glm::ivec3 tnt{8, kBase + 1, 8};
  r.set(tnt, BlockType::TNT);
  r.light(tnt);
  auto zombie = r.game.entities().spawn(EntityKind::TestCreature, {10.5f, kBase + 1.0f, 8.5f});
  auto puppy = r.game.entities().spawn(EntityKind::Puppy, {8.5f, kBase + 1.0f, 10.5f});
  auto item = r.game.entities().spawn(EntityKind::DroppedItem, {6.5f, kBase + 1.0f, 8.5f});
  REQUIRE((zombie && puppy && item));
  r.game.entities().find(*item)->item = {ItemId::RUBY, 1};
  r.game.entities().find(*puppy)->pet = PetOwner{};
  const int zombie_health = r.game.entities().find(*zombie)->health;
  r.ticks(kFuseTicks - 1);
  // Put them back where they were (they wander).
  REQUIRE(r.game.entities().find(*zombie));
  REQUIRE(r.game.entities().find(*puppy));
  r.game.entities().find(*zombie)->body.position = {10.5f, kBase + 1.0f, 8.5f};
  r.game.entities().find(*puppy)->body.position = {8.5f, kBase + 1.0f, 10.5f};
  r.ticks(1);
  const Entity *z = r.game.entities().find(*zombie);
  CHECK((!z || z->health < zombie_health)); // hurt, or killed
  const Entity *p = r.game.entities().find(*puppy);
  REQUIRE(p);
  CHECK(p->shoved_ms > 0);
  CHECK(p->hurt_ms == 0);
  // The dropped ruby is flung, not destroyed.
  CHECK(r.dropped(ItemId::RUBY) == 1);
}
