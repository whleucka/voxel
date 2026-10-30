#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "chunk/chunk_mesh.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include "player/player.hpp"
#include "test_util.hpp"
#include "ui/minimap.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

namespace {

bool inPalette(BlockType b) {
  const auto &palette = paletteItems();
  return std::find(palette.begin(), palette.end(), itemForBlock(b)) != palette.end();
}

constexpr float kDt = 1.0f / 60.0f;
constexpr int kBase = 200; // the rig's floor: clear sky

// Chunk (0,0) with a 12x12 stone floor at y = kBase, x and z in [2, 14).
struct Rig {
  World world;
  Rig() {
    world.loadChunkSync(0, 0);
    fill(world, {2, kBase + 1, 2}, {13, kBase + 6, 13}, BlockType::AIR);
    fill(world, {2, kBase, 2}, {13, kBase, 13}, BlockType::STONE);
  }
  void set(int x, int y, int z, BlockType b) { fill(world, {x, y, z}, {x, y, z}, b); }
};

void steps(Player &p, World &w, int n, bool forward = false) {
  PlayerInput in;
  in.forward = forward; // the camera starts facing -Z (north)
  for (int i = 0; i < n; ++i) {
    p.processKeyboard(kDt, in);
    p.update(kDt, &w);
  }
}

// A body at rest sits within a frame's fall of what it stands on.
bool restsAt(float y, float top) { return std::abs(y - top) < 0.02f; }

constexpr DoorFacing kFacings[] = {DoorFacing::North, DoorFacing::East, DoorFacing::South,
                                   DoorFacing::West};

} // namespace

// ─── Blocks ────────────────────────────────────────────────────────────

TEST_CASE("slab and stair block ids are pinned: saves store them as numbers") {
  CHECK(static_cast<int>(BlockType::OAK_SLAB) == 173);
  CHECK(static_cast<int>(BlockType::SNOW_BRICK_SLAB) == 181);
  CHECK(static_cast<int>(BlockType::OAK_STAIRS_N) == 182);
  CHECK(static_cast<int>(BlockType::SNOW_BRICK_STAIRS_W) == 217);
  CHECK(BlockType::SNOW_BRICK_STAIRS_W < kLastBlockType); // fences came after
}

TEST_CASE("slabs and stairs are named, mined and coloured like their material") {
  for (int m = 0; m < kShapeMaterials; ++m) {
    const BlockType material = kShapeMaterial[m];
    const BlockType slab = slabBlock(m);
    CAPTURE(blockName(material));
    CHECK(isSlab(slab));
    CHECK(shapeMaterial(slab) == material);
    CHECK(blockName(slab) == kSlabNames[m]);
    CHECK(itemDef(itemForBlock(slab)).max_stack == 64);
    CHECK(dropFor(slab) == itemForBlock(slab));
    for (BlockType b : {slab, stairBlock(m, DoorFacing::North), stairBlock(m, DoorFacing::West)}) {
      CHECK(breakMs(b, ItemId::NONE) == breakMs(material, ItemId::NONE));
      CHECK(toughness(b).best == toughness(material).best);
      CHECK(toughness(b).needs == toughness(material).needs);
      CHECK(block_data.at(b).side == block_data.at(material).side);
      CHECK(block_data.at(b).top == block_data.at(material).top);
      CHECK(mapColour(b) == mapColour(material));
      CHECK(blocksMovement(b));
      CHECK(hasOwnShape(b));
      CHECK_FALSE(hasFlatIcon(b));
      CHECK_FALSE(isSolidGround(b)); // a torch would float on the open half
      CHECK(skyLightOpacity(b) == 0);
      CHECK(shouldRenderFace(material, b));
    }
    for (DoorFacing f : kFacings) {
      const BlockType stair = stairBlock(m, f);
      CHECK(isStair(stair));
      CHECK(stairFacing(stair) == f);
      CHECK(shapeMaterial(stair) == material);
      CHECK(blockName(stair) == kStairNames[m]);
      // Whichever way it faces, it's the one stair item.
      CHECK(dropFor(stair) == itemForBlock(stairBlock(m, DoorFacing::North)));
    }
  }
  CHECK(blockName(BlockType::OAK_STAIRS_E) == "Oak Stairs");
  CHECK(blockName(BlockType::COBBLESTONE_SLAB) == "Cobblestone Slab");
}

TEST_CASE("every slab and the north stair are items in the palette; the turned stairs aren't") {
  for (int m = 0; m < kShapeMaterials; ++m) {
    CHECK(inPalette(slabBlock(m)));
    CHECK(inPalette(stairBlock(m, DoorFacing::North)));
    for (DoorFacing f : {DoorFacing::East, DoorFacing::South, DoorFacing::West}) {
      CHECK_FALSE(isValidItem(itemForBlock(stairBlock(m, f))));
      CHECK_FALSE(inPalette(stairBlock(m, f)));
    }
  }
}

// ─── Shapes ────────────────────────────────────────────────────────────

TEST_CASE("a slab fills the bottom half of its cell") {
  CellBoxes boxes = blockBoxes(BlockType::STONE_BRICK_SLAB);
  REQUIRE(boxes.count == 1);
  CHECK(boxes.box[0].lo == glm::vec3(0, 0, 0));
  CHECK(boxes.box[0].hi == glm::vec3(1, 0.5f, 1));
  CHECK(blockHeight(BlockType::STONE_BRICK_SLAB) == 0.5f);
}

TEST_CASE("a stair is a slab with its upper step at the back, the way it faces") {
  for (DoorFacing f : kFacings) {
    CAPTURE(static_cast<int>(f));
    CellBoxes boxes = blockBoxes(stairBlock(0, f));
    REQUIRE(boxes.count == 2);
    CHECK(boxes.box[0].hi == glm::vec3(1, 0.5f, 1));
    const DoorBox step = boxes.box[1];
    CHECK(step.lo.y == 0.5f);
    CHECK(step.hi.y == 1.0f);
    // Half the cell, on the side `facingStep` points to.
    const glm::vec3 size = step.hi - step.lo;
    CHECK(size.x * size.z == doctest::Approx(0.5f));
    const glm::vec3 centre = (step.lo + step.hi) * 0.5f - glm::vec3(0.5f);
    const glm::vec3 toward(facingStep(f));
    CHECK(centre.x == doctest::Approx(toward.x * 0.25f));
    CHECK(centre.z == doctest::Approx(toward.z * 0.25f));
  }
}

TEST_CASE("full blocks are one box, low blocks one low box, and air none") {
  CHECK(blockBoxes(BlockType::STONE).count == 1);
  CHECK(blockBoxes(BlockType::STONE).box[0].hi == glm::vec3(1));
  CHECK(blockBoxes(BlockType::CAKE_4).box[0].hi.y == kCakeHeight);
  CHECK(blockBoxes(BlockType::BED_RED_FOOT_N).box[0].hi.y == kBedHeight);
  CHECK(blockBoxes(BlockType::AIR).count == 0);
  CHECK(blockBoxes(BlockType::POPPY).count == 0);
  CHECK(insideBlock(BlockType::OAK_STAIRS_N, {0.5f, 0.75f, 0.25f}));      // the step
  CHECK_FALSE(insideBlock(BlockType::OAK_STAIRS_N, {0.5f, 0.75f, 0.75f})); // the open half
  CHECK(insideBlock(BlockType::OAK_STAIRS_N, {0.5f, 0.25f, 0.75f}));
}

// ─── Walking on them ───────────────────────────────────────────────────

TEST_CASE("you stand on a slab at half a block, and walk up onto it without jumping") {
  Rig r;
  r.set(8, kBase + 1, 6, BlockType::OAK_SLAB);
  Player drop({8.5f, kBase + 4.0f, 6.5f});
  steps(drop, r.world, 60);
  CHECK(restsAt(drop.getPosition().y, kBase + 1.5f));
  CHECK(drop.isOnGround());

  Player walk({8.5f, kBase + 1.0f, 9.5f});
  steps(walk, r.world, 15); // settle
  steps(walk, r.world, 40, true);
  CHECK(walk.getPosition().z < 7.0f);
  CHECK(restsAt(walk.getPosition().y, kBase + 1.5f));
}

TEST_CASE("you stand on a stair's low step or its high one") {
  Rig r;
  r.set(8, kBase + 1, 6, BlockType::COBBLESTONE_STAIRS_N); // high side at -Z
  Player low({8.5f, kBase + 3.0f, 6.8f});
  steps(low, r.world, 60);
  CHECK(restsAt(low.getPosition().y, kBase + 1.5f));
  Player high({8.5f, kBase + 3.0f, 6.2f});
  steps(high, r.world, 60);
  CHECK(restsAt(high.getPosition().y, kBase + 2.0f));
}

TEST_CASE("a staircase is climbed just by walking") {
  Rig r;
  for (int i = 0; i < 3; ++i) r.set(8, kBase + 1 + i, 6 - i, BlockType::OAK_STAIRS_N);
  for (int i = 0; i < 2; ++i) // the stairs are built on blocks
    for (int y = kBase + 1; y < kBase + 3 - i; ++y) r.set(8, y, 4 + i, BlockType::OAK_PLANKS);
  fill(r.world, {7, kBase + 3, 2}, {9, kBase + 3, 3}, BlockType::OAK_PLANKS); // a landing
  fill(r.world, {7, kBase + 4, 2}, {9, kBase + 5, 2}, BlockType::OAK_PLANKS); // and its wall
  Player p({8.5f, kBase + 1.0f, 9.5f});
  steps(p, r.world, 15);
  steps(p, r.world, 150, true);
  CHECK(p.getPosition().z < 4.0f);
  CHECK(restsAt(p.getPosition().y, kBase + 4.0f));
}

TEST_CASE("a stair's high side is a wall a block tall") {
  Rig r;
  r.set(8, kBase + 1, 6, BlockType::OAK_STAIRS_S); // high side at +Z, facing the walker
  Player p({8.5f, kBase + 1.0f, 9.5f});
  steps(p, r.world, 15);
  steps(p, r.world, 120, true);
  CHECK(p.getPosition().z > 7.0f);
  CHECK(restsAt(p.getPosition().y, kBase + 1.0f));
}

TEST_CASE("a creature's body walks up a stair too") {
  Rig r;
  r.set(8, kBase + 1, 6, BlockType::BRICK_STAIRS_N);
  PhysicsBody b;
  b.half_width = 0.3f;
  b.height = 0.9f;
  b.position = {8.5f, kBase + 1.0f, 8.0f};
  for (int i = 0; i < 60; ++i) { // two blocks: onto its high step
    b.velocity.x = 0.0f;
    b.velocity.z = -2.0f;
    b.step(kDt, &r.world);
  }
  CHECK(b.position.z < 6.5f); // over the high step
  CHECK(restsAt(b.position.y, kBase + 2.0f));
}

// ─── Meshing ───────────────────────────────────────────────────────────

namespace {

// The corners of every quad drawn above the rig's floor wearing `tile`.
std::vector<glm::vec3> cornersWith(World &world, glm::ivec2 tile) {
  auto chunk = world.getChunk(0, 0);
  REQUIRE(chunk);
  ChunkMesh mesh;
  TextureManager unused; // generateCPU makes no GL calls
  mesh.generateCPU(&world, *chunk, unused);
  std::vector<glm::vec3> out;
  for (const BlockVertex &v : mesh.vertexData())
    if (v.tileX == tile.x && v.tileY == tile.y && v.posY > (kBase + 0.5f) * kPosScale)
      out.push_back(glm::vec3(v.posX, v.posY, v.posZ) / kPosScale);
  return out;
}

} // namespace

TEST_CASE("a slab's faces stop halfway up its cell") {
  Rig r;
  r.set(5, kBase + 1, 5, BlockType::COBBLESTONE_SLAB);
  const auto corners = cornersWith(r.world, block_data.at(BlockType::COBBLESTONE).side);
  CHECK(corners.size() == 6 * 4);
  for (glm::vec3 c : corners) {
    CHECK(c.y >= kBase + 1);
    CHECK(c.y <= kBase + 1.5f);
  }
}

TEST_CASE("a stair draws its two boxes, the step at its back") {
  Rig r;
  r.set(5, kBase + 1, 5, BlockType::COBBLESTONE_STAIRS_N); // step toward -Z
  const auto corners = cornersWith(r.world, block_data.at(BlockType::COBBLESTONE).side);
  CHECK(corners.size() == 2 * 6 * 4);
  float top = 0.0f;
  for (glm::vec3 c : corners) {
    top = std::max(top, c.y);
    if (c.y > kBase + 1.5f) CHECK(c.z <= 5.5f); // only the step rises above the slab
  }
  CHECK(top == kBase + 2.0f);
}

TEST_CASE("a full block beside a slab still shows its face") {
  Rig r;
  r.set(5, kBase + 1, 5, BlockType::OAK_SLAB);
  r.set(6, kBase + 1, 5, BlockType::STONE_BRICKS);
  bool faces_slab = false;
  for (glm::vec3 c : cornersWith(r.world, block_data.at(BlockType::STONE_BRICKS).side))
    faces_slab |= c.x == 6.0f && c.y == kBase + 2.0f; // the corners of its -X face
  CHECK(faces_slab);
}
