#include "doctest/doctest.h"
#include "chunk/chunk.hpp"
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
    // A face's four corners, positions doubled; its cell is their minimum.
    glm::ivec3 lo(1 << 30);
    for (size_t k = i; k < i + 4; ++k)
      lo = glm::min(lo, glm::ivec3(v[k].posX, v[k].posY, v[k].posZ) / 2);
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
