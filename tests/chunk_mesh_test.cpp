#include "doctest/doctest.h"
#include "chunk/chunk.hpp"
#include "chunk/chunk_mesh.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <vector>

namespace {

constexpr int kBase = 205; // clear sky above chunks (0, 0) and (1, 0)

struct FaceSeen {
  int sky, block; // the light the face was given
};

// The faces of one kind that chunk (0, 0)'s mesh has for the cell.
std::vector<FaceSeen> facesAt(World &world, glm::ivec3 cell, Face face) {
  auto chunk = world.getChunk(0, 0);
  REQUIRE(chunk);
  ChunkMesh mesh;
  TextureManager unused; // generateCPU makes no GL calls
  mesh.generateCPU(&world, *chunk, unused);
  const auto &v = mesh.vertexData();
  std::vector<FaceSeen> out;
  for (size_t i = 0; i + 3 < v.size(); i += 4) {
    if ((v[i].faceId & 0x7) != face) continue;
    glm::ivec3 lo(1 << 30);
    for (size_t k = i; k < i + 4; ++k)
      lo = glm::min(lo, glm::ivec3(v[k].posX, v[k].posY, v[k].posZ) / int(kPosScale));
    glm::ivec3 c = lo - glm::ivec3(face == Face::Right ? 1 : 0, 0, face == Face::Front ? 1 : 0);
    if (c == cell) out.push_back({(v[i].ao >> 2) & 0xF, (v[i].faceId >> 3) & 0xF});
  }
  return out;
}

} // namespace

TEST_CASE("a face on a chunk's edge goes by the block and light across it") {
  World world;
  world.loadChunkSync(0, 0);
  world.loadChunkSync(1, 0);
  fill(world, {0, kBase - 3, 0}, {2 * kChunkWidth - 1, kBase + 3, kChunkDepth - 1}, BlockType::AIR);
  const glm::ivec3 edge{kChunkWidth - 1, kBase, 8}; // chunk (0, 0)'s +x layer
  world.setBlockAt(edge, BlockType::STONE);

  SUBCASE("open air next door: the face shows, in full sky") {
    auto faces = facesAt(world, edge, Face::Right);
    REQUIRE(faces.size() == 1);
    CHECK(faces[0].sky == 15);
    CHECK(faces[0].block == 0);
  }
  SUBCASE("a lamp next door lights it") {
    world.setBlockAt(edge + glm::ivec3(2, 0, 0), BlockType::GLOWSTONE);
    auto faces = facesAt(world, edge, Face::Right);
    REQUIRE(faces.size() == 1);
    CHECK(faces[0].block == 14);
  }
  SUBCASE("a block next door hides it") {
    world.setBlockAt(edge + glm::ivec3(1, 0, 0), BlockType::STONE);
    CHECK(facesAt(world, edge, Face::Right).empty());
  }
}

TEST_CASE("a face toward a chunk not loaded yet shows") {
  World world;
  world.loadChunkSync(0, 0); // (-1, 0) never loads
  fill(world, {0, kBase - 3, 0}, {kChunkWidth - 1, kBase + 3, kChunkDepth - 1}, BlockType::AIR);
  const glm::ivec3 edge{0, kBase, 8};
  world.setBlockAt(edge, BlockType::STONE);
  auto faces = facesAt(world, edge, Face::Left);
  REQUIRE(faces.size() == 1);
  CHECK(faces[0].sky == 15); // an unloaded chunk counts as open sky
}
