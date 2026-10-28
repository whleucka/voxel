#include "doctest/doctest.h"
#include "chunk/chunk.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk_mesh.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <vector>

namespace {

constexpr int kBase = 200; // clear sky above chunk (0,0)

// For each glow crystal side face in one cell: whether its shards point down,
// i.e. the tile is drawn upside down (its top edge wears the bottom of the tile).
std::vector<bool> crystalSidesAt(World &world, glm::ivec3 cell) {
  auto chunk = world.getChunk(0, 0);
  REQUIRE(chunk);
  ChunkMesh mesh;
  TextureManager unused; // generateCPU doesn't touch it, and makes no GL calls
  mesh.generateCPU(&world, *chunk, unused);
  const glm::ivec2 sideTile = block_data.at(BlockType::GLOW_CRYSTAL).side;
  const auto &v = mesh.vertexData();
  std::vector<bool> down;
  for (size_t i = 0; i + 3 < v.size(); i += 4) {
    int f = v[i].faceId & 0x7;
    if (f == Face::Top || f == Face::Bottom) continue;
    if (v[i].tileX != sideTile.x || v[i].tileY != sideTile.y) continue;
    // A side face's cell is its lowest corner, less the face's own offset.
    glm::ivec3 lo(1 << 30);
    size_t topCorner = i;
    for (size_t k = i; k < i + 4; ++k) {
      lo = glm::min(lo, glm::ivec3(v[k].posX, v[k].posY, v[k].posZ) / int(kPosScale));
      if (v[k].posY > v[topCorner].posY) topCorner = k;
    }
    glm::ivec3 c = lo - glm::ivec3(f == Face::Right ? 1 : 0, 0, f == Face::Front ? 1 : 0);
    if (c == cell) down.push_back(v[topCorner].uvY != 0);
  }
  return down;
}

} // namespace

TEST_CASE("a glow crystal points away from the rock it grows on") {
  World world;
  world.loadChunkSync(0, 0);
  fill(world, {0, kBase - 2, 0}, {15, kBase + 6, 15}, BlockType::AIR);
  fill(world, {1, kBase - 1, 1}, {14, kBase - 1, 14}, BlockType::STONE); // floor

  const std::vector<bool> up(4, false), down(4, true);

  SUBCASE("on a floor it points up") {
    fill(world, {5, kBase, 5}, {5, kBase, 5}, BlockType::GLOW_CRYSTAL);
    CHECK(crystalSidesAt(world, {5, kBase, 5}) == up);
  }
  SUBCASE("under a ceiling it hangs down") {
    fill(world, {5, kBase + 4, 5}, {5, kBase + 4, 5}, BlockType::STONE);
    fill(world, {5, kBase + 3, 5}, {5, kBase + 3, 5}, BlockType::GLOW_CRYSTAL);
    CHECK(crystalSidesAt(world, {5, kBase + 3, 5}) == down);
  }
  SUBCASE("with rock above and below it points up") {
    fill(world, {5, kBase + 1, 5}, {5, kBase + 1, 5}, BlockType::STONE);
    fill(world, {5, kBase, 5}, {5, kBase, 5}, BlockType::GLOW_CRYSTAL);
    CHECK(crystalSidesAt(world, {5, kBase, 5}) == up);
  }
  SUBCASE("a crystal hanging under another one hangs too") {
    fill(world, {5, kBase + 4, 5}, {5, kBase + 4, 5}, BlockType::STONE);
    fill(world, {5, kBase + 2, 5}, {5, kBase + 3, 5}, BlockType::GLOW_CRYSTAL);
    CHECK(crystalSidesAt(world, {5, kBase + 2, 5}) == down);
  }
  SUBCASE("hanging and standing crystals side by side keep their own faces") {
    // A floor crystal under a ceiling one, next to a column of two standing.
    fill(world, {5, kBase + 1, 5}, {6, kBase + 1, 5}, BlockType::STONE);
    fill(world, {5, kBase, 5}, {5, kBase, 5}, BlockType::GLOW_CRYSTAL);       // rock both sides
    fill(world, {6, kBase, 5}, {6, kBase, 5}, BlockType::AIR);
    fill(world, {6, kBase - 1, 5}, {6, kBase - 1, 5}, BlockType::AIR);
    fill(world, {6, kBase, 5}, {6, kBase, 5}, BlockType::GLOW_CRYSTAL);       // hangs
    // The face between them is drawn on both, so each has all four sides.
    CHECK(crystalSidesAt(world, {5, kBase, 5}) == up);
    CHECK(crystalSidesAt(world, {6, kBase, 5}) == down);
  }
}
