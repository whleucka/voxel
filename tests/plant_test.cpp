#include "doctest/doctest.h"
#include "biome/plants.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "chunk/chunk_mesh.hpp"
#include "test_util.hpp"
#include "world/world.hpp"
#include "item/item.hpp"
#include "item/mining.hpp"
#include <algorithm>
#include <set>

namespace {

bool inPalette(BlockType b) {
  const auto &palette = paletteItems();
  return std::find(palette.begin(), palette.end(), itemForBlock(b)) != palette.end();
}

const BlockType kWildPlants[] = {
    BlockType::TALL_GRASS, BlockType::POPPY,       BlockType::BUTTERCUP,
    BlockType::CORNFLOWER, BlockType::PINK_TULIP,  BlockType::DAISY,
    BlockType::LAVENDER,   BlockType::HIBISCUS,    BlockType::FERN,
    BlockType::RED_MUSHROOM, BlockType::BROWN_MUSHROOM, BlockType::DEAD_BUSH,
    BlockType::BERRY_BUSH, BlockType::BERRY_BUSH_BARE, BlockType::SHELLS};

} // namespace

// ─── Blocks ────────────────────────────────────────────────────────────

TEST_CASE("plant block ids are pinned: saves store them as numbers") {
  CHECK(static_cast<int>(BlockType::TALL_GRASS) == 143);
  CHECK(static_cast<int>(BlockType::SHELLS) == 157);
  CHECK(static_cast<int>(BlockType::CACTUS) == 158);
  CHECK(static_cast<int>(BlockType::TORCH) == 162);
  CHECK(static_cast<int>(BlockType::CAKE_4) == 167);
  CHECK(static_cast<int>(BlockType::SNOW_BRICKS) == 172);
  CHECK(kLastBlockType == BlockType::SNOW_BRICKS);
}

TEST_CASE("wild plants are named cutouts you walk through and pick at once") {
  for (BlockType b : kWildPlants) {
    CAPTURE(blockName(b));
    CHECK(isPlant(b));
    CHECK(blockName(b) != "Unknown");
    CHECK_FALSE(blocksMovement(b));
    CHECK(isPassable(b));
    CHECK(isCutout(b));
    CHECK(skyLightOpacity(b) == 0);
    CHECK(hasOwnShape(b));
    CHECK(hasFlatIcon(b));
    CHECK(needsSupport(b));
    CHECK(breakMs(b, ItemId::NONE) == 0);
  }
  CHECK(isFlower(BlockType::POPPY));
  CHECK(isFlower(BlockType::HIBISCUS));
  CHECK_FALSE(isFlower(BlockType::TALL_GRASS));
  CHECK_FALSE(isFlower(BlockType::FERN));
}

TEST_CASE("plants drop themselves, except grass, dead bushes, picked bushes and shells") {
  CHECK(dropFor(BlockType::POPPY) == itemForBlock(BlockType::POPPY));
  CHECK(dropFor(BlockType::RED_MUSHROOM) == itemForBlock(BlockType::RED_MUSHROOM));
  CHECK(dropFor(BlockType::BERRY_BUSH) == itemForBlock(BlockType::BERRY_BUSH));
  CHECK(dropFor(BlockType::BERRY_BUSH_BARE) == itemForBlock(BlockType::BERRY_BUSH));
  CHECK(dropFor(BlockType::TALL_GRASS) == std::nullopt); // the Game rolls for a seed
  CHECK(dropFor(BlockType::DEAD_BUSH) == ItemId::STICKS);
  CHECK(dropFor(BlockType::SHELLS) == ItemId::SEA_SHELL);
}

TEST_CASE("plants are in the creative palette, but not a picked bush or shells") {
  CHECK(inPalette(BlockType::POPPY));
  CHECK(inPalette(BlockType::TALL_GRASS));
  CHECK(inPalette(BlockType::BERRY_BUSH));
  CHECK(inPalette(BlockType::CACTUS));
  CHECK(inPalette(BlockType::BLUE_CORAL));
  CHECK(inPalette(BlockType::SANDSTONE_BRICKS));
  CHECK(inPalette(BlockType::SNOW_BRICKS));
  CHECK_FALSE(inPalette(BlockType::BERRY_BUSH_BARE));
  CHECK_FALSE(inPalette(BlockType::SHELLS));
}

TEST_CASE("each plant grows on its own ground") {
  CHECK(growsOn(BlockType::POPPY, BlockType::GRASS, true));
  CHECK(growsOn(BlockType::TALL_GRASS, BlockType::DIRT, true));
  CHECK_FALSE(growsOn(BlockType::POPPY, BlockType::STONE, true));
  CHECK_FALSE(growsOn(BlockType::POPPY, BlockType::SAND, true));
  CHECK(growsOn(BlockType::BERRY_BUSH, BlockType::GRASS, true));
  CHECK(growsOn(BlockType::DEAD_BUSH, BlockType::SAND, true));
  CHECK_FALSE(growsOn(BlockType::DEAD_BUSH, BlockType::GRASS, true));
  CHECK(growsOn(BlockType::SHELLS, BlockType::SAND, true));
  CHECK(growsOn(BlockType::CACTUS, BlockType::SAND, true));
  CHECK(growsOn(BlockType::CACTUS, BlockType::CACTUS, true));
  CHECK_FALSE(growsOn(BlockType::CACTUS, BlockType::GRASS, true));
  // Mushrooms grow on any solid block, in the dark too.
  CHECK(growsOn(BlockType::BROWN_MUSHROOM, BlockType::STONE, true));
  CHECK_FALSE(growsOn(BlockType::BROWN_MUSHROOM, BlockType::OAK_LEAF, false));
}

TEST_CASE("cactus is solid, and corals and bricks are plain blocks") {
  CHECK(blocksMovement(BlockType::CACTUS));
  CHECK(needsSupport(BlockType::CACTUS));
  CHECK(hasOwnShape(BlockType::CACTUS));
  CHECK_FALSE(isSolidGround(BlockType::CACTUS)); // nothing hangs on it
  CHECK(shouldRenderFace(BlockType::STONE, BlockType::CACTUS));
  for (BlockType b : {BlockType::PINK_CORAL, BlockType::BLUE_CORAL, BlockType::YELLOW_CORAL,
                      BlockType::SANDSTONE_BRICKS, BlockType::SNOW_BRICKS}) {
    CAPTURE(blockName(b));
    CHECK(blocksMovement(b));
    CHECK(isSolidGround(b));
    CHECK_FALSE(isCutout(b));
    CHECK_FALSE(hasOwnShape(b));
    CHECK(dropFor(b) == itemForBlock(b));
  }
  CHECK(isCoral(BlockType::BLUE_CORAL));
}

// ─── Torches ───────────────────────────────────────────────────────────

TEST_CASE("torches glow, are walked through, and all drop a torch") {
  for (int v = static_cast<int>(BlockType::TORCH); v <= static_cast<int>(BlockType::TORCH_W);
       ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(v);
    CHECK(isTorch(b));
    CHECK(blockName(b) == "Torch");
    CHECK(blockLightEmission(b) == 13);
    CHECK_FALSE(blocksMovement(b));
    CHECK(dropFor(b) == itemForBlock(BlockType::TORCH));
    CHECK(breakMs(b, ItemId::NONE) == 0);
  }
  CHECK(isValidItem(itemForBlock(BlockType::TORCH)));
  CHECK_FALSE(isValidItem(itemForBlock(BlockType::TORCH_N)));
  CHECK(inPalette(BlockType::TORCH));
}

TEST_CASE("a torch goes on a floor or a wall, never a ceiling") {
  CHECK(torchFor({0, -1, 0}) == BlockType::TORCH);
  CHECK(torchFor({0, 0, -1}) == BlockType::TORCH_N);
  CHECK(torchFor({1, 0, 0}) == BlockType::TORCH_E);
  CHECK(torchFor({0, 0, 1}) == BlockType::TORCH_S);
  CHECK(torchFor({-1, 0, 0}) == BlockType::TORCH_W);
  CHECK(torchFor({0, 1, 0}) == std::nullopt);
  for (glm::ivec3 d : {glm::ivec3(0, -1, 0), glm::ivec3(0, 0, -1), glm::ivec3(1, 0, 0),
                       glm::ivec3(0, 0, 1), glm::ivec3(-1, 0, 0)})
    CHECK(torchSupport(*torchFor(d)) == d);
}

// ─── Cake ──────────────────────────────────────────────────────────────

TEST_CASE("a cake counts its slices down and is placed whole") {
  CHECK(cakeSlices(BlockType::CAKE_4) == 4);
  CHECK(cakeSlices(BlockType::CAKE_1) == 1);
  for (int n = 1; n <= kCakeSlices; ++n) CHECK(cakeSlices(cakeBlock(n)) == n);
  CHECK(blockName(BlockType::CAKE_2) == "Cake");
  CHECK(blocksMovement(BlockType::CAKE_4));
  CHECK(blockHeight(BlockType::CAKE_4) == kCakeHeight);
  CHECK(dropFor(BlockType::CAKE_4) == std::nullopt); // eaten, not picked up
  CHECK(isValidItem(itemForBlock(BlockType::CAKE_4)));
  CHECK_FALSE(isValidItem(itemForBlock(BlockType::CAKE_3)));
  CHECK(itemDef(itemForBlock(BlockType::CAKE_4)).name == "Cake");
}

// ─── Meshing ───────────────────────────────────────────────────────────

namespace {

constexpr int kSky = 200; // clear sky above chunk (0,0)

// The mesh's vertices that wear `tile`, and how far up and across they reach.
struct Drawn {
  int quads = 0;
  glm::vec3 lo{1e9f}, hi{-1e9f};
};
Drawn drawnWith(World &world, glm::ivec2 tile) {
  auto chunk = world.getChunk(0, 0);
  REQUIRE(chunk);
  ChunkMesh mesh;
  TextureManager unused; // generateCPU makes no GL calls
  mesh.generateCPU(&world, *chunk, unused);
  Drawn d;
  const auto &v = mesh.vertexData();
  for (size_t i = 0; i + 3 < v.size(); i += 4) {
    if (v[i].tileX != tile.x || v[i].tileY != tile.y) continue;
    ++d.quads;
    for (size_t k = i; k < i + 4; ++k) {
      glm::vec3 p = glm::vec3(v[k].posX, v[k].posY, v[k].posZ) / float(kPosScale);
      d.lo = glm::min(d.lo, p);
      d.hi = glm::max(d.hi, p);
    }
  }
  return d;
}

World &openGround() {
  static World world;
  world.loadChunkSync(0, 0);
  fill(world, {0, kSky - 2, 0}, {15, kSky + 6, 15}, BlockType::AIR);
  fill(world, {1, kSky - 1, 1}, {14, kSky - 1, 14}, BlockType::GRASS);
  return world;
}

} // namespace

TEST_CASE("a plant is drawn as crossed planes inside its cell") {
  World &world = openGround();
  fill(world, {5, kSky, 5}, {5, kSky, 5}, BlockType::POPPY);
  Drawn d = drawnWith(world, block_data.at(BlockType::POPPY).side);
  CHECK(d.quads == 16); // four planes, front and back, as boxes with no depth
  CHECK(d.lo == glm::vec3(5, kSky, 5));
  CHECK(d.hi == glm::vec3(6, kSky + 1, 6));
}

TEST_CASE("a torch on a wall stands against it, higher up") {
  World &world = openGround();
  fill(world, {5, kSky, 4}, {5, kSky + 1, 4}, BlockType::STONE);
  fill(world, {5, kSky + 1, 5}, {5, kSky + 1, 5}, BlockType::TORCH_N);
  Drawn d = drawnWith(world, kTorchTile);
  REQUIRE(d.quads > 0);
  CHECK(d.lo.y > kSky + 1);        // raised off the cell's floor
  CHECK(d.lo.z < 5.0f);            // leaning into the wall's side
}

TEST_CASE("a cactus is a box a little inside its cell") {
  World &world = openGround();
  fill(world, {5, kSky - 1, 5}, {5, kSky - 1, 5}, BlockType::SAND);
  fill(world, {5, kSky, 5}, {5, kSky, 5}, BlockType::CACTUS);
  Drawn d = drawnWith(world, kCactusSideTile);
  CHECK(d.quads == 4);
  CHECK(d.lo.x == doctest::Approx(5 + 1.0f / 16));
  CHECK(d.hi.x == doctest::Approx(6 - 1.0f / 16));
  CHECK(d.hi.y == doctest::Approx(kSky + 1));
}

TEST_CASE("an eaten cake is shorter and shows where it was cut") {
  World &world = openGround();
  fill(world, {5, kSky, 5}, {5, kSky, 5}, BlockType::CAKE_4);
  CHECK(drawnWith(world, kCakeCutTile).quads == 0);
  const float whole = drawnWith(world, kCakeTopTile).hi.x;
  fill(world, {5, kSky, 5}, {5, kSky, 5}, BlockType::CAKE_2);
  CHECK(drawnWith(world, kCakeCutTile).quads == 1);
  Drawn half = drawnWith(world, kCakeTopTile);
  CHECK(half.hi.x < whole);
  CHECK(half.hi.y == doctest::Approx(kSky + kCakeHeight));
}

// ─── What grows where ──────────────────────────────────────────────────

TEST_CASE("compost sprouts each biome's own plants on its own ground") {
  std::mt19937 rng(7);
  auto sprouts = [&](BiomeType biome, BlockType ground) {
    std::set<BlockType> got;
    for (int i = 0; i < 300; ++i)
      if (auto p = plants::sprout(biome, ground, rng)) got.insert(*p);
    return got;
  };
  using enum BlockType;
  CHECK(sprouts(BiomeType::PLAINS, GRASS) ==
        std::set<BlockType>{TALL_GRASS, POPPY, BUTTERCUP, CORNFLOWER});
  CHECK(sprouts(BiomeType::CHERRY, GRASS) == std::set<BlockType>{TALL_GRASS, PINK_TULIP, DAISY});
  CHECK(sprouts(BiomeType::MOUNTAIN, GRASS) == std::set<BlockType>{FERN, LAVENDER});
  CHECK(sprouts(BiomeType::TROPICAL, GRASS) == std::set<BlockType>{TALL_GRASS, HIBISCUS});
  CHECK(sprouts(BiomeType::DESERT, SAND) == std::set<BlockType>{DEAD_BUSH, CACTUS});
  CHECK(sprouts(BiomeType::OCEAN, SAND) == std::set<BlockType>{SHELLS});
  CHECK(sprouts(BiomeType::DESERT, GRASS).empty());
  CHECK(sprouts(BiomeType::MOUNTAIN, SAND).empty());
  CHECK(sprouts(BiomeType::PLAINS, STONE).empty());
}

TEST_CASE("every flower colour grows wild somewhere") {
  std::set<BlockType> flowers;
  for (BiomeType b : {BiomeType::PLAINS, BiomeType::MOUNTAIN, BiomeType::DESERT,
                      BiomeType::OCEAN, BiomeType::TROPICAL, BiomeType::CHERRY})
    for (BlockType f : plants::wildFor(b).flowers) flowers.insert(f);
  for (int v = static_cast<int>(BlockType::POPPY); v <= static_cast<int>(BlockType::HIBISCUS); ++v)
    CHECK(flowers.count(static_cast<BlockType>(v)) == 1);
}
