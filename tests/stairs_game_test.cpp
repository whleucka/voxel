// Slabs and stairs in the Game: a stair turns to the way you look, and two
// slabs make a block, through a LocalConnection as Engine drives it.
#include "doctest/doctest.h"
#include "block/block_data.hpp"
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
    fill(game.world(), {0, kBase + 1, 0}, {15, kBase + 12, 15}, BlockType::AIR);
    look(-90.0f);
  }

  PlayerState &ps() { return *game.findPlayer(id); }
  std::vector<Event> run(Command cmd) {
    a.send(std::move(cmd));
    game.pump();
    return pollAll(a);
  }
  void look(float yaw) {
    MovePlayer m;
    m.position = kStand;
    m.yaw = yaw;
    m.on_ground = true;
    m.respawn = ps().respawn;
    run(m);
  }
  void hold(BlockType b, uint16_t n = 1) { ps().inventory.set(ps().selected_slot, {itemForBlock(b), n}); }
  int held() { return ps().inventory.slot(ps().selected_slot).count; }
  BlockType at(glm::ivec3 p) const { return game.world().getBlockAt(glm::vec3(p) + 0.5f); }
  std::vector<Event> place(glm::ivec3 pos, glm::ivec3 against = {0, -1, 0}) {
    return run(PlaceBlock{pos, against});
  }
};

const glm::ivec3 kUp{0, 1, 0};
const glm::ivec3 kSpot{4, kBase + 1, 4}; // in reach of the player

} // namespace

TEST_CASE("a stair is placed with its high side away from you") {
  const std::pair<float, DoorFacing> looks[] = {{-90.0f, DoorFacing::North},
                                                {0.0f, DoorFacing::East},
                                                {90.0f, DoorFacing::South},
                                                {180.0f, DoorFacing::West}};
  for (auto [yaw, facing] : looks) {
    CAPTURE(yaw);
    Yard y;
    y.look(yaw);
    y.hold(BlockType::OAK_STAIRS_N);
    CHECK(reason(y.place(kSpot)) == "");
    CHECK(y.at(kSpot) == stairBlock(0, facing));
    CHECK(y.held() == 0);
  }
}

TEST_CASE("a slab on top of the same slab makes the full block") {
  Yard y;
  y.hold(BlockType::STONE_BRICK_SLAB, 2);
  y.place(kSpot);
  REQUIRE(y.at(kSpot) == BlockType::STONE_BRICK_SLAB);
  // Clicking its top aims at the cell above, against the slab below.
  CHECK(reason(y.place(kSpot + kUp)) == "");
  CHECK(y.at(kSpot) == BlockType::STONE_BRICKS);
  CHECK(y.at(kSpot + kUp) == BlockType::AIR);
  CHECK(y.held() == 0);
}

TEST_CASE("a different slab on top goes in the cell above") {
  Yard y;
  y.hold(BlockType::OAK_SLAB);
  y.place(kSpot);
  y.hold(BlockType::BRICK_SLAB);
  CHECK(reason(y.place(kSpot + kUp)) == "");
  CHECK(y.at(kSpot) == BlockType::OAK_SLAB);
  CHECK(y.at(kSpot + kUp) == BlockType::BRICK_SLAB);
}

TEST_CASE("a slab placed against another's side goes beside it") {
  Yard y;
  y.hold(BlockType::OAK_SLAB, 2);
  y.place(kSpot);
  const glm::ivec3 beside = kSpot + glm::ivec3(1, 0, 0);
  CHECK(reason(y.place(beside, {-1, 0, 0})) == "");
  CHECK(y.at(kSpot) == BlockType::OAK_SLAB);
  CHECK(y.at(beside) == BlockType::OAK_SLAB);
}

TEST_CASE("two slabs don't make a block round someone standing on them") {
  Yard y;
  const glm::ivec3 under_me(2, kBase + 1, 2);
  y.game.world().setBlockAt(under_me, BlockType::OAK_SLAB);
  y.hold(BlockType::OAK_SLAB);
  CHECK(reason(y.place(under_me + kUp)) == "place: inside a player");
  CHECK(y.at(under_me) == BlockType::OAK_SLAB);
}

TEST_CASE("a stair breaks like its material and drops the one stair item") {
  Yard y;
  y.look(0.0f);
  y.hold(BlockType::COBBLESTONE_STAIRS_N);
  y.place(kSpot);
  REQUIRE(y.at(kSpot) == BlockType::COBBLESTONE_STAIRS_E);
  CHECK(dropFor(y.at(kSpot)) == itemForBlock(BlockType::COBBLESTONE_STAIRS_N));
  CHECK(breakMs(BlockType::COBBLESTONE_STAIRS_E, ItemId::STONE_PICKAXE) ==
        breakMs(BlockType::COBBLESTONE, ItemId::STONE_PICKAXE));
  CHECK_FALSE(dropsWith(BlockType::COBBLESTONE_STAIRS_E, ItemId::NONE)); // needs a pickaxe
}
