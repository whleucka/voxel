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
  std::vector<Event> light(glm::ivec3 p) {
    holdTorch();
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
