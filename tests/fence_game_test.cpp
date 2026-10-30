// Fences, gates and ladders in the Game: placing, opening and shutting a
// gate, ladders falling off, and falls onto a ladder.
#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "entity/entity_store.hpp"
#include "item/mining.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"

namespace {

constexpr int kBase = 200; // the yard's floor: clear sky above

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

std::string reason(const std::vector<Event> &evs) {
  auto r = find<CommandRejected>(evs);
  return r ? r->reason : "";
}

// A stone yard in the sky over chunk (0,0), with one survival player in it.
struct Yard {
  ScopedRenderDistance rd{0};
  Game game;
  LocalConnection a;
  PlayerId id = 0;
  static inline const glm::vec3 kStand{2.5f, kBase + 1.0f, 2.5f};

  Yard() {
    game.spawner() = Spawner{};
    game.setMode(GameMode::Survival);
    game.addClient(a);
    a.send(Join{});
    game.pump();
    auto joined = find<Joined>(pollAll(a));
    REQUIRE(joined);
    id = joined->id;
    game.world().loadChunkSync(0, 0);
    fill(game.world(), {0, kBase, 0}, {15, kBase, 15}, BlockType::STONE);
    fill(game.world(), {0, kBase + 1, 0}, {15, kBase + 30, 15}, BlockType::AIR);
    move(kStand, -90.0f);
  }

  PlayerState &ps() { return *game.findPlayer(id); }
  std::vector<Event> run(Command cmd) {
    a.send(std::move(cmd));
    game.pump();
    return pollAll(a);
  }
  std::vector<Event> move(glm::vec3 at, float yaw = -90.0f, bool on_ground = true) {
    MovePlayer m;
    m.position = at;
    m.yaw = yaw;
    m.on_ground = on_ground;
    m.respawn = ps().respawn;
    return run(m);
  }
  void hold(BlockType b, uint16_t n = 1) { ps().inventory.set(ps().selected_slot, {itemForBlock(b), n}); }
  void holdItem(ItemId id) { ps().inventory.set(ps().selected_slot, {id, 1}); }
  int held() { return ps().inventory.slot(ps().selected_slot).count; }
  BlockType at(glm::ivec3 p) const { return game.world().getBlockAt(glm::vec3(p) + 0.5f); }
  std::vector<Event> place(glm::ivec3 pos, glm::ivec3 against = {0, -1, 0}) {
    return run(PlaceBlock{pos, against});
  }
  // Survival mining: start, wait it out, finish.
  std::vector<Event> mine(glm::ivec3 pos) {
    run(StartBreak{pos});
    int ms = breakMs(at(pos), ps().inventory.slot(ps().selected_slot).id);
    for (int t = 0; t < ms; t += Game::kTickMs) game.tick();
    pollAll(a);
    return run(BreakBlock{pos});
  }
  int dropped(ItemId item) const {
    int n = 0;
    for (const auto &[eid, e] : game.entities().all())
      if (e.kind == EntityKind::DroppedItem && e.item.id == item) n += e.item.count;
    return n;
  }
};

const glm::ivec3 kSpot{4, kBase + 1, 4}; // in reach of the player
const glm::ivec3 kNorth{0, 0, -1};

} // namespace

// ─── Ladders ───────────────────────────────────────────────────────────

TEST_CASE("a ladder goes on the side of a block, turned to hang on it") {
  const std::pair<glm::ivec3, BlockType> sides[] = {{{0, 0, -1}, BlockType::LADDER_N},
                                                    {{1, 0, 0}, BlockType::LADDER_E},
                                                    {{0, 0, 1}, BlockType::LADDER_S},
                                                    {{-1, 0, 0}, BlockType::LADDER_W}};
  for (auto [toward, ladder] : sides) {
    Yard y;
    y.game.world().setBlockAt(kSpot + toward, BlockType::STONE_BRICKS);
    y.hold(BlockType::LADDER_N);
    CHECK(reason(y.place(kSpot, toward)) == "");
    CHECK(y.at(kSpot) == ladder);
    CHECK(y.held() == 0);
  }
}

TEST_CASE("a ladder won't go on a floor, a ceiling or nothing") {
  Yard y;
  y.hold(BlockType::LADDER_N, 3);
  CHECK(reason(y.place(kSpot, {0, -1, 0})) == "place: ladders go on the side of a block");
  CHECK(reason(y.place(kSpot, kNorth)) == "place: it can't go there"); // air to the north
  y.game.world().setBlockAt(kSpot + kNorth, BlockType::OAK_FENCE);
  CHECK(reason(y.place(kSpot, kNorth)) == "place: it can't go there"); // not a full block
  CHECK(y.at(kSpot) == BlockType::AIR);
  CHECK(y.held() == 3);
}

TEST_CASE("a ladder drops off when the block it hangs on is broken") {
  Yard y;
  const glm::ivec3 wall = kSpot + kNorth;
  y.game.world().setBlockAt(wall, BlockType::DIRT);
  y.hold(BlockType::LADDER_N);
  y.place(kSpot, kNorth);
  REQUIRE(y.at(kSpot) == BlockType::LADDER_N);
  y.ps().inventory.set(y.ps().selected_slot, {});
  CHECK(reason(y.mine(wall)) == "");
  CHECK(y.at(kSpot) == BlockType::AIR);
  CHECK(y.dropped(itemForBlock(BlockType::LADDER_N)) == 1);
}

TEST_CASE("a long fall onto a ladder doesn't hurt") {
  Yard y;
  const glm::ivec3 wall{6, kBase + 1, 6};
  fill(y.game.world(), wall, wall + glm::ivec3(0, 20, 0), BlockType::STONE);
  fill(y.game.world(), wall + glm::ivec3(0, 0, 1), wall + glm::ivec3(0, 20, 1), BlockType::LADDER_N);
  const int health = y.ps().vitals.health();
  // Falling past the top of the ladder from high up, then sliding down it.
  const glm::vec3 x{6.5f, 0.0f, 7.5f};
  y.move(x + glm::vec3(0, kBase + 25.0f, 0), -90.0f, false);
  for (float h = kBase + 24.0f; h >= kBase + 1.0f; h -= 0.5f)
    y.move(x + glm::vec3(0, h, 0), -90.0f, h <= kBase + 1.0f);
  CHECK(y.ps().vitals.health() == health);

  // The same fall with no ladder does.
  y.move(glm::vec3(10.5f, kBase + 25.0f, 10.5f), -90.0f, false);
  for (float h = kBase + 24.0f; h >= kBase + 1.0f; h -= 0.5f)
    y.move(glm::vec3(10.5f, h, 10.5f), -90.0f, h <= kBase + 1.0f);
  CHECK(y.ps().vitals.health() < health);
}

// ─── Gates ─────────────────────────────────────────────────────────────

TEST_CASE("a gate goes across the way you look, shut") {
  const std::pair<float, BlockType> looks[] = {{-90.0f, BlockType::FENCE_GATE_X_CLOSED},
                                               {0.0f, BlockType::FENCE_GATE_Z_CLOSED},
                                               {90.0f, BlockType::FENCE_GATE_X_CLOSED},
                                               {180.0f, BlockType::FENCE_GATE_Z_CLOSED}};
  for (auto [yaw, gate] : looks) {
    CAPTURE(yaw);
    Yard y;
    y.move(Yard::kStand, yaw);
    y.hold(BlockType::FENCE_GATE_X_CLOSED);
    CHECK(reason(y.place(kSpot)) == "");
    CHECK(y.at(kSpot) == gate);
  }
}

TEST_CASE("right-clicking a gate opens and shuts it, for everyone to see") {
  Yard y;
  y.game.world().setBlockAt(kSpot, BlockType::FENCE_GATE_Z_CLOSED);
  auto evs = y.run(UseDoor{kSpot});
  CHECK(reason(evs) == "");
  CHECK(y.at(kSpot) == BlockType::FENCE_GATE_Z_OPEN);
  auto changed = find<BlockChanged>(evs);
  REQUIRE(changed);
  CHECK(changed->type == BlockType::FENCE_GATE_Z_OPEN);
  y.run(UseDoor{kSpot});
  CHECK(y.at(kSpot) == BlockType::FENCE_GATE_Z_CLOSED);
}

TEST_CASE("a gate won't shut on someone standing in it, and needs reach") {
  Yard y;
  y.game.world().setBlockAt(kSpot, BlockType::FENCE_GATE_X_OPEN);
  y.move(glm::vec3(kSpot) + glm::vec3(0.5f, 0.0f, 0.5f));
  CHECK(reason(y.run(UseDoor{kSpot})) == "door: someone's in the way");
  CHECK(y.at(kSpot) == BlockType::FENCE_GATE_X_OPEN);

  y.move(Yard::kStand);
  const glm::ivec3 far{14, kBase + 1, 14};
  y.game.world().setBlockAt(far, BlockType::FENCE_GATE_X_CLOSED);
  CHECK(reason(y.run(UseDoor{far})) == "door: out of reach");
}

TEST_CASE("a fence and a gate break like planks and drop themselves") {
  Yard y;
  y.game.world().setBlockAt(kSpot, BlockType::FENCE_GATE_Z_OPEN);
  y.ps().inventory.set(y.ps().selected_slot, {});
  CHECK(reason(y.mine(kSpot)) == "");
  CHECK(y.at(kSpot) == BlockType::AIR);
  int gates = y.dropped(itemForBlock(BlockType::FENCE_GATE_X_CLOSED));
  for (const ItemStack &s : y.ps().inventory.slots())
    gates += s.id == itemForBlock(BlockType::FENCE_GATE_X_CLOSED) ? s.count : 0;
  CHECK(gates == 1);
}

// ─── Pens ──────────────────────────────────────────────────────────────

TEST_CASE("a woolly in a fenced pen with its gate shut stays in") {
  Yard y;
  // A 5x5 pen from (5,5) to (11,11): fences round the edge, the gate on the south side.
  for (int i = 5; i <= 11; ++i)
    for (glm::ivec3 c : {glm::ivec3(i, kBase + 1, 5), glm::ivec3(i, kBase + 1, 11),
                         glm::ivec3(5, kBase + 1, i), glm::ivec3(11, kBase + 1, i)})
      y.game.world().setBlockAt(c, BlockType::OAK_FENCE);
  y.game.world().setBlockAt({8, kBase + 1, 11}, BlockType::FENCE_GATE_X_CLOSED);
  auto w = y.game.entities().spawn(EntityKind::Woolly, {8.5f, kBase + 1.0f, 8.5f});
  REQUIRE(w);
  for (int i = 0; i < 20 * 120; ++i) { // two minutes
    y.game.tick();
    const glm::vec3 p = y.game.entities().find(*w)->body.position;
    REQUIRE(p.x > 5.5f);
    REQUIRE(p.x < 11.5f);
    REQUIRE(p.z > 5.5f);
    REQUIRE(p.z < 11.5f);
  }
}
