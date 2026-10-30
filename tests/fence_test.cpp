// Fences, the fence gate and ladders: blocks, recipes, shapes, bodies,
// climbing, meshing and paths.
#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "chunk/chunk_mesh.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include "item/recipes.hpp"
#include "physics/path.hpp"
#include "player/player.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include <algorithm>
#include <cmath>

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr int kBase = 200; // the rig's floor: clear sky

const BlockType kFences[] = {BlockType::OAK_FENCE, BlockType::PINE_FENCE, BlockType::PALM_FENCE,
                             BlockType::CHERRY_FENCE};
const BlockType kPlanks[] = {BlockType::OAK_PLANKS, BlockType::PINE_PLANKS,
                             BlockType::PALM_PLANKS, BlockType::CHERRY_PLANKS};

bool inPalette(BlockType b) {
  const auto &palette = paletteItems();
  return std::find(palette.begin(), palette.end(), itemForBlock(b)) != palette.end();
}

const Recipe &recipeFor(ItemId id) {
  for (const Recipe &r : recipes())
    if (r.makes.id == id) return r;
  FAIL("no recipe makes that");
  return recipes()[0];
}

bool needs(const Recipe &r, ItemId id, int count) {
  for (const Ingredient &in : r.needs)
    if (std::find(in.any_of.begin(), in.any_of.end(), id) != in.any_of.end())
      return in.count == count;
  return false;
}

// Chunk (0,0) with a 12x12 stone floor at y = kBase, x and z in [2, 14).
struct Rig {
  World world;
  Rig() {
    world.loadChunkSync(0, 0);
    fill(world, {2, kBase + 1, 2}, {13, kBase + 8, 13}, BlockType::AIR);
    fill(world, {2, kBase, 2}, {13, kBase, 13}, BlockType::STONE);
  }
  void set(int x, int y, int z, BlockType b) { fill(world, {x, y, z}, {x, y, z}, b); }
  // A fence across the floor at z, from x 2 to 13.
  void fenceRow(int z, BlockType fence = BlockType::OAK_FENCE) {
    fill(world, {2, kBase + 1, z}, {13, kBase + 1, z}, fence);
  }
};

PlayerInput walking(bool forward = true, bool jump = false, bool crouch = false) {
  PlayerInput in;
  in.forward = forward; // the camera starts facing -Z (north)
  in.jump = jump;
  in.crouch = crouch;
  return in;
}

void steps(Player &p, World &w, int n, PlayerInput in = PlayerInput{}) {
  for (int i = 0; i < n; ++i) {
    p.processKeyboard(kDt, in);
    p.update(kDt, &w);
  }
}

} // namespace

// ─── Blocks and items ──────────────────────────────────────────────────

TEST_CASE("fence, gate and ladder block ids are pinned: saves store them as numbers") {
  CHECK(static_cast<int>(BlockType::OAK_FENCE) == 218);
  CHECK(static_cast<int>(BlockType::CHERRY_FENCE) == 221);
  CHECK(static_cast<int>(BlockType::FENCE_GATE_X_CLOSED) == 222);
  CHECK(static_cast<int>(BlockType::FENCE_GATE_Z_OPEN) == 225);
  CHECK(static_cast<int>(BlockType::LADDER_N) == 226);
  CHECK(static_cast<int>(BlockType::LADDER_W) == 229);
  CHECK(kLastBlockType == BlockType::LADDER_W);
}

TEST_CASE("fences, the gate and ladders are named, mined and dropped") {
  for (int w = 0; w < kFenceWoods; ++w) {
    CAPTURE(w);
    CHECK(blockName(kFences[w]) == kFenceNames[w]);
    CHECK(dropFor(kFences[w]) == itemForBlock(kFences[w]));
    CHECK(toughness(kFences[w]).best == ToolKind::Axe);
    CHECK(breakMs(kFences[w], ItemId::NONE) == breakMs(kPlanks[w], ItemId::NONE));
    CHECK(block_data.at(kFences[w]).side == block_data.at(kPlanks[w]).side);
  }
  for (int v = static_cast<int>(BlockType::FENCE_GATE_X_CLOSED);
       v <= static_cast<int>(BlockType::FENCE_GATE_Z_OPEN); ++v) {
    BlockType gate = static_cast<BlockType>(v);
    CHECK(blockName(gate) == "Fence Gate");
    CHECK(dropFor(gate) == itemForBlock(BlockType::FENCE_GATE_X_CLOSED));
    CHECK(toughness(gate).best == ToolKind::Axe);
  }
  for (int v = static_cast<int>(BlockType::LADDER_N); v <= static_cast<int>(BlockType::LADDER_W);
       ++v) {
    BlockType ladder = static_cast<BlockType>(v);
    CHECK(blockName(ladder) == "Ladder");
    CHECK(dropFor(ladder) == itemForBlock(BlockType::LADDER_N));
    CHECK(breakMs(ladder, ItemId::WOOD_AXE) < breakMs(ladder, ItemId::NONE));
    CHECK(breakMs(ladder, ItemId::NONE) < breakMs(BlockType::OAK_PLANKS, ItemId::NONE));
    CHECK(block_data.at(ladder).side == kLadderTile);
    CHECK(needsSupport(ladder));
  }
  CHECK(hasFlatIcon(BlockType::LADDER_N));
}

TEST_CASE("each fence, one gate and one ladder are items in the palette; the turned ones aren't") {
  for (BlockType f : kFences) {
    CHECK(isValidItem(itemForBlock(f)));
    CHECK(inPalette(f));
  }
  CHECK(inPalette(BlockType::FENCE_GATE_X_CLOSED));
  CHECK(inPalette(BlockType::LADDER_N));
  for (BlockType b : {BlockType::FENCE_GATE_X_OPEN, BlockType::FENCE_GATE_Z_CLOSED,
                      BlockType::FENCE_GATE_Z_OPEN, BlockType::LADDER_E, BlockType::LADDER_S,
                      BlockType::LADDER_W})
    CHECK_FALSE(isValidItem(itemForBlock(b)));
}

TEST_CASE("fences, a gate and ladders are made at a table from planks and sticks") {
  for (int w = 0; w < kFenceWoods; ++w) {
    CAPTURE(w);
    const Recipe &r = recipeFor(itemForBlock(kFences[w]));
    CHECK(r.makes.count == 3);
    CHECK(r.needs_table);
    CHECK(needs(r, itemForBlock(kPlanks[w]), 4));
    CHECK(needs(r, ItemId::STICKS, 2));
  }
  const Recipe &gate = recipeFor(itemForBlock(BlockType::FENCE_GATE_X_CLOSED));
  CHECK(gate.makes.count == 1);
  CHECK(gate.needs_table);
  for (BlockType p : kPlanks) CHECK(needs(gate, itemForBlock(p), 2)); // any wood
  CHECK(needs(gate, ItemId::STICKS, 4));
  const Recipe &ladder = recipeFor(itemForBlock(BlockType::LADDER_N));
  CHECK(ladder.makes.count == 3);
  CHECK(ladder.needs_table);
  CHECK(needs(ladder, ItemId::STICKS, 7));
}

// ─── Shapes ────────────────────────────────────────────────────────────

TEST_CASE("a lone fence is a post too tall to jump; rails reach to what it joins") {
  Rig r;
  r.set(6, kBase + 1, 6, BlockType::OAK_FENCE);
  const glm::ivec3 at{6, kBase + 1, 6};
  CellBoxes lone = r.world.boxesAt(at);
  REQUIRE(lone.count == 1);
  CHECK(lone.box[0].hi.y == kFenceHeight);
  CHECK(lone.box[0].hi.x - lone.box[0].lo.x < 0.5f);

  r.set(6, kBase + 1, 5, BlockType::PINE_FENCE);          // north: any wood joins
  r.set(7, kBase + 1, 6, BlockType::STONE_BRICKS);        // east: a wall joins
  r.set(6, kBase + 1, 7, BlockType::FENCE_GATE_X_CLOSED); // south: a gate across the line doesn't
  r.set(5, kBase + 1, 6, BlockType::FENCE_GATE_X_CLOSED); // west: one in line does
  CellBoxes joined = r.world.boxesAt(at);
  CHECK(joined.count == 4);
  bool reaches_south = false;
  for (const DoorBox &b : joined) reaches_south |= b.hi.z == 1.0f;
  CHECK_FALSE(reaches_south);

  r.set(6, kBase + 1, 7, BlockType::FENCE_GATE_Z_OPEN); // an open gate in line still joins
  CHECK(r.world.boxesAt(at).count == 5);
}

TEST_CASE("a closed gate is a bar across its cell; an open one and a ladder are walked through") {
  CellBoxes x = blockBoxes(BlockType::FENCE_GATE_X_CLOSED);
  REQUIRE(x.count == 1);
  CHECK(x.box[0].lo.x == 0.0f);
  CHECK(x.box[0].hi.x == 1.0f);
  CHECK(x.box[0].hi.y == kFenceHeight);
  CellBoxes z = blockBoxes(BlockType::FENCE_GATE_Z_CLOSED);
  REQUIRE(z.count == 1);
  CHECK(z.box[0].hi.z - z.box[0].lo.z == 1.0f);
  CHECK(blockBoxes(BlockType::FENCE_GATE_X_OPEN).count == 0);
  CHECK(blockBoxes(BlockType::LADDER_E).count == 0);
  CHECK_FALSE(blocksMovement(BlockType::LADDER_N));
  CHECK_FALSE(isSolidGround(BlockType::OAK_FENCE)); // nothing grows or stands on it
  CHECK(skyLightOpacity(BlockType::OAK_FENCE) == 0);
  CHECK(skyLightOpacity(BlockType::LADDER_N) == 0);
}

// ─── Bodies ────────────────────────────────────────────────────────────

TEST_CASE("you can't walk or jump over a fence") {
  Rig r;
  r.fenceRow(6);
  Player p({8.5f, kBase + 1.0f, 9.5f});
  steps(p, r.world, 15);
  steps(p, r.world, 240, walking(true, true));
  CHECK(p.getPosition().z > 6.5f);
}

TEST_CASE("a creature's body can't hop a fence either") {
  Rig r;
  r.fenceRow(6);
  PhysicsBody b;
  b.half_width = 0.3f;
  b.height = 0.9f;
  b.position = {8.5f, kBase + 1.0f, 8.5f};
  for (int i = 0; i < 240; ++i) {
    b.velocity.x = 0.0f;
    b.velocity.z = -3.0f;
    if (b.on_ground && i % 20 == 0) b.velocity.y = kPlayerJumpStrength;
    b.step(kDt, &r.world);
  }
  CHECK(b.position.z > 6.5f);
}

TEST_CASE("a closed gate stops you; an open one lets you through") {
  Rig r;
  r.fenceRow(6);
  r.set(8, kBase + 1, 6, BlockType::FENCE_GATE_X_CLOSED);
  Player p({8.5f, kBase + 1.0f, 9.5f});
  steps(p, r.world, 15);
  steps(p, r.world, 180, walking());
  CHECK(p.getPosition().z > 6.5f);

  r.set(8, kBase + 1, 6, BlockType::FENCE_GATE_X_OPEN);
  steps(p, r.world, 180, walking());
  CHECK(p.getPosition().z < 5.0f);
}

// ─── Climbing ──────────────────────────────────────────────────────────

namespace {

// A stone wall 5 blocks high at z 5 (its top at kBase + 6), with a ladder
// up its south face at x 8.
struct Tower : Rig {
  static constexpr float kTop = kBase + 6.0f;
  Tower() {
    fill(world, {2, kBase + 1, 5}, {13, kBase + 5, 5}, BlockType::STONE);
    fill(world, {8, kBase + 1, 6}, {8, kBase + 5, 6}, BlockType::LADDER_N);
  }
};

} // namespace

TEST_CASE("you walk into a ladder's cell, up to its wall") {
  Tower t;
  Player p({8.5f, kBase + 1.0f, 9.5f});
  steps(p, t.world, 15);
  steps(p, t.world, 120, walking());
  CHECK(p.getPosition().z < 6.5f);
  CHECK(p.getPosition().z > 5.9f);
}

TEST_CASE("pushing into a ladder climbs it, and you step off the top") {
  Tower t;
  Player p({8.5f, kBase + 1.0f, 6.5f});
  steps(p, t.world, 30, walking());
  CHECK(p.getPosition().y > kBase + 1.5f); // going up
  for (int i = 0; i < 400 && p.getPosition().z > 5.5f; ++i) steps(p, t.world, 1, walking());
  steps(p, t.world, 30); // and stand there
  CHECK(p.getPosition().z < 5.9f); // over the top
  CHECK(std::abs(p.getPosition().y - Tower::kTop) < 0.05f);
}

TEST_CASE("jumping climbs too, crouching holds on, and letting go slides down slowly") {
  Tower t;
  Player p({8.5f, kBase + 1.0f, 6.5f});
  steps(p, t.world, 60, walking(false, true));
  const float high = p.getPosition().y;
  CHECK(high > kBase + 2.5f);

  steps(p, t.world, 60, walking(false, false, true));
  CHECK(std::abs(p.getPosition().y - high) < 0.05f);

  steps(p, t.world, 30); // half a second
  const float slid = high - p.getPosition().y;
  CHECK(slid > 0.5f);
  CHECK(slid < 1.0f); // a fall would be several blocks
}

TEST_CASE("creatures don't climb ladders") {
  Tower t;
  PhysicsBody b;
  b.half_width = 0.3f;
  b.height = 0.9f;
  b.position = {8.5f, kBase + 1.0f, 6.5f};
  for (int i = 0; i < 120; ++i) {
    b.velocity.z = -2.0f;
    b.step(kDt, &t.world);
  }
  CHECK(b.position.y < kBase + 1.1f);
}

// ─── Meshing ───────────────────────────────────────────────────────────

namespace {

// How many boxes (6 quads, 24 corners each) above the rig's floor wear `tile`.
size_t boxesWith(World &world, glm::ivec2 tile) {
  auto chunk = world.getChunk(0, 0);
  REQUIRE(chunk);
  ChunkMesh mesh;
  TextureManager unused; // generateCPU makes no GL calls
  mesh.generateCPU(&world, *chunk, unused);
  size_t corners = 0;
  for (const BlockVertex &v : mesh.vertexData())
    if (v.tileX == tile.x && v.tileY == tile.y && v.posY > (kBase + 0.5f) * kPosScale)
      ++corners;
  return corners / 24;
}

} // namespace

TEST_CASE("a lone fence draws its post; joined fences draw two rails each way") {
  Rig r;
  const glm::ivec2 pine = block_data.at(BlockType::PINE_PLANKS).side;
  r.set(5, kBase + 1, 5, BlockType::PINE_FENCE);
  CHECK(boxesWith(r.world, pine) == 1);
  r.set(6, kBase + 1, 5, BlockType::PINE_FENCE);
  CHECK(boxesWith(r.world, pine) == 2 * (1 + 2));
}

TEST_CASE("a shut gate draws posts, rails and an upright; a ladder draws its panel") {
  Rig r;
  r.set(5, kBase + 1, 5, BlockType::FENCE_GATE_Z_CLOSED);
  CHECK(boxesWith(r.world, block_data.at(BlockType::OAK_PLANKS).side) == 5);
  r.set(5, kBase + 1, 5, BlockType::FENCE_GATE_Z_OPEN);
  CHECK(boxesWith(r.world, block_data.at(BlockType::OAK_PLANKS).side) == 6);
  r.set(8, kBase + 1, 5, BlockType::STONE);
  r.set(8, kBase + 1, 6, BlockType::LADDER_N);
  CHECK(boxesWith(r.world, kLadderTile) == 1);
}

// ─── Paths ─────────────────────────────────────────────────────────────

TEST_CASE("no path goes over a fence, but one goes through an open gate") {
  Rig r;
  r.fenceRow(6);
  PathParams p;
  const glm::ivec3 from{8, kBase + 1, 10}, to{8, kBase + 1, 3};
  CHECK_FALSE(findPath(r.world, from, to, p));
  r.set(10, kBase + 1, 6, BlockType::FENCE_GATE_X_OPEN);
  CHECK(findPath(r.world, from, to, p));
}
