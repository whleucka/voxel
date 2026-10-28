#include "doctest/doctest.h"
#include "chunk/chunk.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk_mesh.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <set>

namespace {

constexpr int kBase = 200; // clear sky above chunk (0,0)

// Which faces (by faceId) the chunk's blended mesh has around one cell.
std::multiset<int> waterFacesAt(World &world, glm::ivec3 cell) {
  auto chunk = world.getChunk(0, 0);
  REQUIRE(chunk);
  ChunkMesh mesh;
  TextureManager unused; // generateCPU doesn't touch it, and makes no GL calls
  mesh.generateCPU(&world, *chunk, unused);
  const auto &v = mesh.transparentVertexData();
  std::multiset<int> faces;
  for (size_t i = 0; i + 3 < v.size(); i += 4) {
    // A face's four corners, positions scaled; its cell is their minimum.
    glm::ivec3 lo(1 << 30);
    for (size_t k = i; k < i + 4; ++k)
      lo = glm::min(lo, glm::ivec3(v[k].posX, v[k].posY, v[k].posZ) / int(kPosScale));
    // Top faces sit at y + 1, +X at x + 1, +Z at z + 1.
    int f = v[i].faceId;
    glm::ivec3 c = lo - glm::ivec3(f == 3 ? 1 : 0, f == 0 ? 1 : 0, f == 4 ? 1 : 0);
    if (c == cell) faces.insert(f);
  }
  return faces;
}

} // namespace

TEST_CASE("a pool beside open air shows its sides; one walled in shows only its top") {
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {0, kBase - 2, 0}, {15, kBase + 4, 15}, BlockType::AIR);
  fill(world, {2, kBase - 1, 2}, {12, kBase - 1, 12}, BlockType::STONE); // the floor

  // A cave pool: one block of water on the floor, open air all round.
  glm::ivec3 pool{5, kBase, 5};
  fill(world, pool, pool, BlockType::WATER);
  std::multiset<int> faces = waterFacesAt(world, pool);
  CHECK(faces.count(Face::Top) == 1);
  CHECK(faces.count(Face::Left) == 1);
  CHECK(faces.count(Face::Right) == 1);
  CHECK(faces.count(Face::Front) == 1);
  CHECK(faces.count(Face::Back) == 1);
  CHECK(faces.count(Face::Bottom) == 0); // it sits on rock

  // Walled in by stone on three sides and more water on the fourth: top only.
  fill(world, {4, kBase, 4}, {6, kBase, 6}, BlockType::STONE);
  fill(world, pool, pool + glm::ivec3(0, 0, 1), BlockType::WATER);
  faces = waterFacesAt(world, pool);
  CHECK(faces.count(Face::Top) == 1);
  CHECK(faces.size() == 1);
}

TEST_CASE("water at the edge of the loaded world doesn't wall itself off") {
  World world;
  world.loadChunkSync(0, 0); // chunk (1, 0), past +X, isn't loaded
  fill(world, {0, kBase - 2, 0}, {15, kBase + 4, 15}, BlockType::AIR);
  fill(world, {12, kBase - 1, 2}, {15, kBase - 1, 12}, BlockType::STONE);
  fill(world, {12, kBase, 2}, {15, kBase, 12}, BlockType::STONE);
  glm::ivec3 edge{15, kBase, 5};
  fill(world, edge, edge, BlockType::WATER);
  fill(world, edge + glm::ivec3(0, 0, 1), edge + glm::ivec3(0, 0, 1), BlockType::AIR);
  std::multiset<int> faces = waterFacesAt(world, edge);
  CHECK(faces.count(Face::Right) == 0); // toward the unloaded chunk
  CHECK(faces.count(Face::Front) == 1); // toward the air beside it
}

TEST_CASE("a door is drawn as its thin panel, and its neighbours keep their faces") {
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {0, kBase - 2, 0}, {15, kBase + 4, 15}, BlockType::AIR);
  fill(world, {2, kBase - 1, 2}, {12, kBase - 1, 12}, BlockType::STONE);
  const glm::ivec3 cell{5, kBase, 5};
  for (bool open : {false, true}) {
    CAPTURE(open);
    BlockType door = doorBlock(false, DoorFacing::North, open);
    fill(world, cell, cell, door);
    fill(world, cell + glm::ivec3(1, 0, 0), cell + glm::ivec3(1, 0, 0), BlockType::STONE);

    auto chunk = world.getChunk(0, 0);
    ChunkMesh mesh;
    TextureManager unused;
    mesh.generateCPU(&world, *chunk, unused);
    // The door's corners: everything strictly inside its cell's span.
    glm::vec3 lo(1e9f), hi(-1e9f);
    int corners = 0;
    bool stone_face_toward_door = false;
    for (const BlockVertex &v : mesh.vertexData()) {
      glm::vec3 p = glm::vec3(v.posX, v.posY, v.posZ) / kPosScale;
      if ((v.faceId & 7) == Face::Left && p.x == cell.x + 1 && p.y >= cell.y &&
          p.y <= cell.y + 1 && p.z >= cell.z && p.z <= cell.z + 1)
        stone_face_toward_door = true;
      if (v.tileY != kDoorBottomTile.y || v.tileX < kDoorBottomTile.x ||
          v.tileX > kDoorEdgeTile.x)
        continue;
      ++corners;
      lo = glm::min(lo, p);
      hi = glm::max(hi, p);
    }
    CHECK(corners == 24); // six faces
    DoorBox box = doorPanel(door);
    CHECK(lo == glm::vec3(cell) + box.lo);
    CHECK(hi == glm::vec3(cell) + box.hi);
    CHECK(stone_face_toward_door);
  }
}

TEST_CASE("a bed is drawn low, with its pillow at the head end, and its neighbours keep their faces") {
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {0, kBase - 2, 0}, {15, kBase + 4, 15}, BlockType::AIR);
  fill(world, {2, kBase - 1, 2}, {12, kBase - 1, 12}, BlockType::STONE);
  const glm::ivec3 foot{5, kBase, 5}, head{6, kBase, 5}; // facing east
  fill(world, foot, foot, bedBlock(BedColour::Green, false, DoorFacing::East));
  fill(world, head, head, bedBlock(BedColour::Green, true, DoorFacing::East));
  fill(world, head + glm::ivec3(1, 0, 0), head + glm::ivec3(1, 0, 0), BlockType::STONE);

  auto chunk = world.getChunk(0, 0);
  ChunkMesh mesh;
  TextureManager unused;
  mesh.generateCPU(&world, *chunk, unused);
  const glm::ivec2 blanket = bedBlanketTile(BedColour::Green);
  glm::vec3 lo(1e9f), hi(-1e9f), pillow_lo(1e9f), pillow_hi(-1e9f);
  int blanket_corners = 0;
  bool stone_face_toward_bed = false;
  for (const BlockVertex &v : mesh.vertexData()) {
    glm::vec3 p = glm::vec3(v.posX, v.posY, v.posZ) / kPosScale;
    glm::ivec2 tile(v.tileX, v.tileY);
    if ((v.faceId & 7) == Face::Left && p.x == head.x + 1 && p.y >= head.y &&
        p.y <= head.y + 1 && p.z >= head.z && p.z <= head.z + 1)
      stone_face_toward_bed = true;
    if (tile != kBedFrameTile && tile != kBedPillowTile && tile != blanket) continue;
    lo = glm::min(lo, p);
    hi = glm::max(hi, p);
    blanket_corners += tile == blanket;
    if (tile == kBedPillowTile) {
      pillow_lo = glm::min(pillow_lo, p);
      pillow_hi = glm::max(pillow_hi, p);
    }
  }
  CHECK(lo == glm::vec3(foot));
  CHECK(hi == glm::vec3(foot) + glm::vec3(2.0f, kBedHeight, 1.0f));
  CHECK(blanket_corners > 0);
  // The pillow sits on the head's far half.
  CHECK(pillow_lo.x >= head.x + 0.5f);
  CHECK(pillow_hi.x < head.x + 1.0f);
  CHECK(pillow_hi.y == doctest::Approx(head.y + kBedHeight));
  CHECK(stone_face_toward_bed);
}
