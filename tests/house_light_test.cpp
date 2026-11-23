#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "chunk/chunk_mesh.hpp"
#include "render/texture_manager.hpp"
#include "world/world.hpp"
#include <algorithm>

// A lamp in a house lights the house, not the garden: light can't cross a
// closed door's panel or a slab's or stair's full face, though it gets into
// their cells (they're lit from inside them).

namespace {

constexpr int kFloor = kChunkHeight - 20; // clear sky above chunk (0, 0)
constexpr int bit(int d) { return 1 << d; }
enum Step { kWest, kEast, kDown, kUp, kNorth, kSouth }; // -x +x -y +y -z +z

// A hollow 7 x 6 x 7 room of planks; `roof` over it and `door` (two halves,
// in the west wall) if given.
struct House {
  glm::ivec3 lo, hi, mid;
  House(glm::ivec3 at) : lo(at), hi(at + glm::ivec3(6, 5, 6)), mid((lo + hi) / 2) {}
  bool shell(glm::ivec3 p) const {
    return p.x == lo.x || p.x == hi.x || p.y == lo.y || p.y == hi.y || p.z == lo.z ||
           p.z == hi.z;
  }
  bool inside(glm::ivec3 p) const {
    return glm::all(glm::greaterThanEqual(p, lo)) && glm::all(glm::lessThanEqual(p, hi));
  }
  glm::ivec3 doorAt() const { return {lo.x, lo.y + 1, mid.z}; }

  // Every block of it, in order: walls, then the lamp last (or first).
  std::vector<std::pair<glm::ivec3, BlockType>> blocks(BlockType roof, BlockType door,
                                                       bool lamp_first = false) const {
    std::vector<std::pair<glm::ivec3, BlockType>> out;
    if (lamp_first) out.push_back({mid, BlockType::GLOWSTONE});
    for (int x = lo.x; x <= hi.x; ++x)
      for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z) {
          glm::ivec3 p{x, y, z};
          if (!shell(p)) continue;
          BlockType b = y == hi.y ? roof : BlockType::OAK_PLANKS;
          out.push_back({p, b});
        }
    if (door != BlockType::AIR) {
      for (auto &[p, b] : out) {
        if (p == doorAt()) b = door;
        if (p == doorAt() + glm::ivec3(0, 1, 0))
          b = doorBlock(true, doorFacing(door), doorOpen(door));
      }
    }
    if (!lamp_first) out.push_back({mid, BlockType::GLOWSTONE});
    return out;
  }

  // The brightest block light in the three blocks around the house.
  int leak(const World &world) const {
    int worst = 0;
    for (int x = lo.x - 3; x <= hi.x + 3; ++x)
      for (int y = lo.y - 3; y <= hi.y + 3; ++y)
        for (int z = lo.z - 3; z <= hi.z + 3; ++z)
          if (!inside({x, y, z})) worst = std::max<int>(worst, world.getBlockLightAt({x, y, z}));
    return worst;
  }
};

// The house built block by block, as a player would.
int builtLeak(House h, BlockType roof, BlockType door, bool lamp_first = false) {
  World world;
  for (int cx = -1; cx <= 1; ++cx)
    for (int cz = -1; cz <= 1; ++cz) world.loadChunkSync(cx, cz);
  for (const auto &[p, b] : h.blocks(roof, door, lamp_first)) world.setBlockAt(p, b);
  CHECK(world.getBlockLightAt(h.mid + glm::ivec3(1, 0, 0)) == 14); // lit inside
  return h.leak(world);
}

int floorDiv(int a) { return a >= 0 ? a / kChunkWidth : -((-a + kChunkWidth - 1) / kChunkWidth); }

} // namespace

TEST_CASE("closed doors, slabs and stairs cover the sides they fill edge to edge") {
  // A closed door's panel: the side it's against. Open, it covers nothing.
  CHECK(lightWalls(doorBlock(false, DoorFacing::West, false)) == bit(kEast));
  CHECK(lightWalls(doorBlock(true, DoorFacing::East, false)) == bit(kWest));
  CHECK(lightWalls(doorBlock(false, DoorFacing::North, false)) == bit(kSouth));
  CHECK(lightWalls(doorBlock(false, DoorFacing::South, false)) == bit(kNorth));
  CHECK(lightWalls(doorBlock(false, DoorFacing::West, true)) == 0);
  // A slab its bottom; a top slab its top; a stair its bottom and its back.
  CHECK(lightWalls(BlockType::OAK_SLAB) == bit(kDown));
  CHECK(lightWalls(BlockType::OAK_TOP_SLAB) == bit(kUp));
  CHECK(lightWalls(BlockType::OAK_STAIRS_N) == (bit(kDown) | bit(kNorth)));
  CHECK(lightWalls(BlockType::OAK_STAIRS_E) == (bit(kDown) | bit(kEast)));
  // Everything else: light's stopped (or not) by opacity alone.
  for (BlockType b : {BlockType::AIR, BlockType::GLASS, BlockType::OAK_PLANKS,
                      BlockType::OAK_FENCE, BlockType::TORCH, BlockType::OAK_LEAF})
    CHECK(lightWalls(b) == 0);
  CHECK(lightCrosses(BlockType::AIR, BlockType::OAK_SLAB, kDown));       // in at its open top...
  CHECK_FALSE(lightCrosses(BlockType::AIR, BlockType::OAK_SLAB, kUp));    // ...not its bottom
  CHECK_FALSE(lightCrosses(BlockType::OAK_SLAB, BlockType::AIR, kDown));  // nor out of it
}

TEST_CASE("a lamp in a sealed house lights nothing outside it") {
  const House h({3, kFloor, 3});
  CHECK(builtLeak(h, BlockType::OAK_PLANKS, BlockType::AIR) == 0);
  CHECK(builtLeak(h, BlockType::OAK_PLANKS, BlockType::AIR, true) == 0);
  // Across chunk borders too.
  CHECK(builtLeak(House({-3, kFloor, -3}), BlockType::OAK_PLANKS, BlockType::AIR) == 0);
}

TEST_CASE("light stays in behind a closed door and under a slab or stair roof") {
  const House h({3, kFloor, 3});
  // The door in the west wall, put in by someone inside looking out (west) or
  // outside looking in (east): its panel is on the outside or the inside.
  for (DoorFacing f : {DoorFacing::West, DoorFacing::East}) {
    CAPTURE(static_cast<int>(f));
    CHECK(builtLeak(h, BlockType::OAK_PLANKS, doorBlock(false, f, false)) == 0);
    CHECK(builtLeak(h, BlockType::OAK_PLANKS, doorBlock(false, f, false), true) == 0);
  }
  CHECK(builtLeak(h, BlockType::OAK_SLAB, BlockType::AIR) == 0);
  CHECK(builtLeak(h, BlockType::OAK_STAIRS_N, BlockType::AIR) == 0);
  // Across a chunk border, with the door on it.
  const House border({0, kFloor, -3}); // its west wall is the x = 0 border
  CHECK(builtLeak(border, BlockType::OAK_SLAB, doorBlock(false, DoorFacing::West, false)) == 0);
}

TEST_CASE("an open door, a window or a leaf roof still let light out") {
  const House h({3, kFloor, 3});
  // (As does a roof of top slabs: on the walls, it leaves a half-block slit.)
  CHECK(builtLeak(h, BlockType::OAK_TOP_SLAB, BlockType::AIR) > 0);
  CHECK(builtLeak(h, BlockType::OAK_PLANKS, doorBlock(false, DoorFacing::West, true)) > 8);
  CHECK(builtLeak(h, BlockType::GLASS, BlockType::AIR) > 8);
  CHECK(builtLeak(h, BlockType::OAK_LEAF, BlockType::AIR) > 8);
}

TEST_CASE("a saved house loads with its light kept in, whichever chunk comes first") {
  const House h({0, kFloor, -3}); // across the x = 0 and z = 0 borders
  WorldEdits edits;
  for (const auto &[p, b] :
       h.blocks(BlockType::OAK_SLAB, doorBlock(false, DoorFacing::West, false))) {
    const int cx = floorDiv(p.x), cz = floorDiv(p.z);
    const int lx = p.x - cx * kChunkWidth, lz = p.z - cz * kChunkDepth;
    edits[{cx, cz}][lx + kChunkWidth * (lz + kChunkDepth * p.y)] = static_cast<uint8_t>(b);
  }
  std::vector<std::pair<int, int>> order;
  for (int cx = -1; cx <= 1; ++cx)
    for (int cz = -1; cz <= 1; ++cz) order.push_back({cx, cz});
  for (int pass = 0; pass < 2; ++pass) {
    World world;
    world.setLoadedEdits(edits);
    for (auto [cx, cz] : order) world.loadChunkSync(cx, cz);
    CHECK(world.getBlockLightAt(h.mid + glm::ivec3(1, 0, 0)) == 14);
    CHECK(h.leak(world) == 0);
    std::reverse(order.begin(), order.end());
  }
}

TEST_CASE("no sky comes in through a slab roof") {
  World world;
  world.loadChunkSync(0, 0);
  const House h({3, kFloor, 3});
  for (const auto &[p, b] : h.blocks(BlockType::OAK_SLAB, BlockType::AIR))
    if (b != BlockType::GLOWSTONE) world.setBlockAt(p, b);
  CHECK(world.getSkyLightAt(h.mid) == 0);
  CHECK(world.getSkyLightAt(h.hi) == 15); // the roof slabs themselves are in the sky
}

TEST_CASE("a closed door is dark on its outside and lit on its inside") {
  World world;
  for (int cx = -1; cx <= 1; ++cx)
    for (int cz = -1; cz <= 1; ++cz) world.loadChunkSync(cx, cz);
  const House h({3, kFloor, 3});
  // A West door: its panel is against the outside (west) of its cell.
  const BlockType door = doorBlock(false, DoorFacing::East, false);
  REQUIRE(lightWalls(door) == bit(kWest));
  for (const auto &[p, b] : h.blocks(BlockType::OAK_PLANKS, door)) world.setBlockAt(p, b);

  auto chunk = world.getChunk(0, 0);
  ChunkMesh mesh;
  TextureManager unused; // generateCPU makes no GL calls
  mesh.generateCPU(&world, *chunk, unused);
  const glm::ivec2 tile = block_data.at(door).side;
  int outside = 0, inside = 0;
  for (const BlockVertex &v : mesh.vertexData()) {
    if (v.tileX != tile.x || v.tileY != tile.y) continue;
    const int face = v.faceId & 0x7, light = (v.faceId >> 3) & 0xF;
    if (face == Face::Left) { ++outside; CHECK(light == 0); }   // faces the garden
    if (face == Face::Right) { ++inside; CHECK(light > 10); }   // faces the lamp
  }
  CHECK(outside == 4);
  CHECK(inside == 4);
}
