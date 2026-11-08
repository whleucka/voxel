// Water in the Game: a block taken out of water leaves water, not air, and
// water runs into the space a break opens beside or under it.
#include "doctest/doctest.h"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"

namespace {

constexpr int kBase = 200; // the yard's floor: clear sky above

// A stone yard in the sky over chunk (0,0), with a pool in it two deep
// (x and z 4..6, y kBase+1..kBase+2), and one creative player beside it.
struct Pool {
  ScopedRenderDistance rd{0};
  Game game;
  LocalConnection a;

  Pool() {
    game.spawner() = Spawner{};
    game.setMode(GameMode::Creative);
    game.addClient(a);
    a.send(Join{});
    game.pump();
    std::vector<Event> evs;
    a.poll(evs);
    game.world().loadChunkSync(0, 0);
    fill(game.world(), {0, kBase, 0}, {15, kBase, 15}, BlockType::STONE);
    fill(game.world(), {0, kBase + 1, 0}, {15, kBase + 12, 15}, BlockType::AIR);
    fill(game.world(), {4, kBase + 1, 4}, {6, kBase + 2, 6}, BlockType::WATER);
    MovePlayer m;
    m.position = {3.5f, kBase + 1.0f, 3.5f};
    m.on_ground = true;
    a.send(m);
    game.pump();
    a.poll(evs);
  }

  BlockType at(glm::ivec3 p) const { return game.world().getBlockAt(glm::vec3(p) + 0.5f); }
  void set(glm::ivec3 p, BlockType b) { fill(game.world(), p, p, b); }
  void breakAt(glm::ivec3 p) {
    a.send(BreakBlock{p});
    game.pump();
  }
  // One step of flowing water.
  void flow() {
    for (int i = 0; i < 5; ++i) game.tick();
  }
};

} // namespace

TEST_CASE("a block broken in water leaves water") {
  Pool p;
  const glm::ivec3 cell{5, kBase + 1, 5};
  p.set(cell, BlockType::DIRT); // as if placed in the pool
  p.breakAt(cell);
  CHECK(p.at(cell) == BlockType::WATER);
}

TEST_CASE("a block broken at the water's surface leaves water") {
  Pool p;
  const glm::ivec3 cell{5, kBase + 2, 5};
  p.set(cell, BlockType::DIRT);
  p.breakAt(cell);
  CHECK(p.at(cell) == BlockType::WATER);
}

TEST_CASE("a block broken away from water leaves air") {
  Pool p;
  const glm::ivec3 cell{2, kBase + 1, 6};
  p.set(cell, BlockType::DIRT);
  p.breakAt(cell);
  CHECK(p.at(cell) == BlockType::AIR);
  p.flow();
  CHECK(p.at(cell) == BlockType::AIR);
}

TEST_CASE("water runs into a trench dug from it, a cell a step") {
  Pool p;
  // A wall of the pool, then a trench beyond it.
  const glm::ivec3 wall{7, kBase + 1, 5};
  const glm::ivec3 trench{8, kBase + 1, 5};
  p.set(wall, BlockType::DIRT);
  p.breakAt(wall);
  CHECK(p.at(wall) == BlockType::WATER);
  CHECK(p.at(trench) == BlockType::AIR); // not yet
  p.flow();
  CHECK(p.at(trench) == BlockType::WATER);
  // It levels out where it came from: nothing rises above the pool's top.
  for (int i = 0; i < 20; ++i) p.flow();
  CHECK(p.at({8, kBase + 3, 5}) == BlockType::AIR);
  CHECK(p.at({7, kBase + 3, 5}) == BlockType::AIR);
}

TEST_CASE("water falls through a hole broken under it") {
  Pool p;
  const glm::ivec3 floor{5, kBase, 5};
  const glm::ivec3 below = floor - glm::ivec3(0, 1, 0);
  p.set(below, BlockType::AIR);
  p.set(below - glm::ivec3(0, 1, 0), BlockType::STONE); // something to land on
  p.breakAt(floor);
  CHECK(p.at(floor) == BlockType::WATER);
  p.flow();
  CHECK(p.at(below) == BlockType::WATER);
}

TEST_CASE("water doesn't spread out across the air beside a fall") {
  Pool p;
  // An open drop beside the pool's wall, nothing under it for a way.
  const glm::ivec3 wall{7, kBase + 1, 5};
  const glm::ivec3 beyond{8, kBase + 1, 5};
  fill(p.game.world(), {8, kBase - 6, 5}, {8, kBase, 5}, BlockType::AIR);
  p.set(wall, BlockType::DIRT);
  p.breakAt(wall);
  REQUIRE(p.at(wall) == BlockType::WATER);
  for (int i = 0; i < 100; ++i) p.flow(); // it floods the yard round the drop
  CHECK(p.at(beyond) == BlockType::AIR);
  CHECK_FALSE(p.game.waterFlowing());
}

TEST_CASE("a cave dug into from under water fills slowly, from the floor up") {
  Pool p;
  // A closed 3x3x3 room under the pool's floor.
  fill(p.game.world(), {3, kBase - 4, 3}, {7, kBase - 1, 7}, BlockType::STONE);
  fill(p.game.world(), {4, kBase - 3, 4}, {6, kBase - 1, 6}, BlockType::AIR);
  auto count = [&](int y) {
    int n = 0;
    for (int x = 4; x <= 6; ++x)
      for (int z = 4; z <= 6; ++z) n += p.at({x, y, z}) == BlockType::WATER;
    return n;
  };
  p.breakAt({5, kBase, 5});
  REQUIRE(p.at({5, kBase, 5}) == BlockType::WATER);
  int steps = 0;
  while (count(kBase - 3) + count(kBase - 2) + count(kBase - 1) < 27 && steps < 100) {
    p.flow();
    ++steps;
    // Nothing fills beside the falling water until the layer under it is full.
    const int mid = count(kBase - 2) - (p.at({5, kBase - 2, 5}) == BlockType::WATER);
    const int top = count(kBase - 1) - (p.at({5, kBase - 1, 5}) == BlockType::WATER);
    if (mid > 0) CHECK(count(kBase - 3) == 9);
    if (top > 0) CHECK(count(kBase - 2) == 9);
  }
  CHECK(count(kBase - 3) + count(kBase - 2) + count(kBase - 1) == 27);
  CHECK(steps >= 27 / 8); // a few cells a step, not all at once
  for (int i = 0; i < 5; ++i) p.flow();
  CHECK_FALSE(p.game.waterFlowing());
}
