// Characterization tests for World block edits. These pin down today's
// behaviour so the sim-core refactor can be checked against it.
//
// Tests never call World::init() or World::update(): those load shaders and
// upload meshes, which needs a GL context. Mesh building itself is CPU-only.
#include "doctest/doctest.h"
#include "core/constants.hpp"
#include "test_util.hpp"
#include "block/block_data.hpp"
#include "core/seed.hpp"
#include "core/settings.hpp"
#include "world/world.hpp"
#include <chrono>
#include <queue>
#include <thread>

namespace {

// Highest non-air block in a column, or -1.
int topSolidY(const World &world, int bx, int bz) {
  for (int y = kChunkHeight - 1; y >= 0; --y) {
    BlockType b = world.getBlockAt(glm::vec3(bx + 0.5f, y + 0.5f, bz + 0.5f));
    if (b != BlockType::AIR && b != BlockType::WATER) return y;
  }
  return -1;
}

uint32_t linearIndex(int lx, int y, int lz) {
  return lx + kChunkWidth * (lz + kChunkDepth * y);
}

} // namespace

TEST_CASE("placing a block is readable and recorded as an edit") {
  World world;
  world.loadChunkSync(0, 0);

  const glm::ivec3 pos{3, kChunkHeight - 2, 5};
  REQUIRE(world.getBlockAt(glm::vec3(pos) + 0.5f) == BlockType::AIR);

  world.setBlockAt(pos, BlockType::STONE);

  CHECK(world.getBlockAt(glm::vec3(pos) + 0.5f) == BlockType::STONE);
  WorldEdits edits = world.snapshotEdits();
  REQUIRE(edits.count({0, 0}) == 1);
  CHECK(edits[{0, 0}].at(linearIndex(3, pos.y, 5)) ==
        static_cast<uint8_t>(BlockType::STONE));
}

TEST_CASE("breaking a block records AIR") {
  World world;
  world.loadChunkSync(0, 0);

  int y = topSolidY(world, 7, 7);
  REQUIRE(y > 0);
  world.setBlockAt({7, y, 7}, BlockType::AIR);

  CHECK(world.getBlockAt(glm::vec3(7.5f, y + 0.5f, 7.5f)) == BlockType::AIR);
  WorldEdits edits = world.snapshotEdits();
  CHECK(edits[{0, 0}].at(linearIndex(7, y, 7)) ==
        static_cast<uint8_t>(BlockType::AIR));
}

TEST_CASE("edits outside the height range or loaded chunks are ignored") {
  World world;
  world.loadChunkSync(0, 0);

  world.setBlockAt({1, -1, 1}, BlockType::STONE);
  world.setBlockAt({1, kChunkHeight, 1}, BlockType::STONE);
  world.setBlockAt({100 * kChunkWidth, 10, 0}, BlockType::STONE); // unloaded

  CHECK(world.snapshotEdits().empty());
}

TEST_CASE("saved edits are replayed onto freshly loaded chunks") {
  WorldEdits saved;
  saved[{0, 0}][linearIndex(2, kChunkHeight - 2, 2)] =
      static_cast<uint8_t>(BlockType::SAND);

  World world;
  world.setLoadedEdits(saved);
  world.loadChunkSync(0, 0);

  CHECK(world.getBlockAt(glm::vec3(2.5f, kChunkHeight - 1.5f, 2.5f)) ==
        BlockType::SAND);
}

TEST_CASE("placing a light emitter lights its neighbourhood") {
  World world;
  world.loadChunkSync(0, 0);

  const glm::ivec3 pos{8, kChunkHeight - 4, 8};
  REQUIRE(world.getBlockLightAt(pos + glm::ivec3(1, 0, 0)) == 0);

  world.setBlockAt(pos, BlockType::GLOWSTONE);
  CHECK(world.getBlockLightAt(pos + glm::ivec3(1, 0, 0)) > 0);

  world.setBlockAt(pos, BlockType::AIR);
  CHECK(world.getBlockLightAt(pos + glm::ivec3(1, 0, 0)) == 0);
}

TEST_CASE("chunks stream around every interest point") {
  ScopedRenderDistance rd(2);
  World world;

  const glm::vec3 points[] = {
      {0.5f, 80.0f, 0.5f},
      {100.5f * kChunkWidth, 80.0f, 0.5f},
  };
  world.setInterestPoints(points);

  CHECK(waitForChunk(world, 0, 0));
  CHECK(waitForChunk(world, 1, 1));
  CHECK(waitForChunk(world, 100, 0));
  CHECK(waitForChunk(world, 99, -1));
  // Nothing in between is loaded.
  CHECK(world.getChunk(50, 0) == nullptr);
}

TEST_CASE("chunks far from every interest point are unloaded") {
  ScopedRenderDistance rd(2);
  World world;

  const glm::vec3 here[] = {{0.5f, 80.0f, 0.5f}};
  world.setInterestPoints(here);
  REQUIRE(waitForChunk(world, 0, 0));

  const glm::vec3 there[] = {{100.5f * kChunkWidth, 80.0f, 0.5f}};
  world.setInterestPoints(there);
  REQUIRE(waitForChunk(world, 100, 0));

  // Generation of the old area may still be in flight; poll until it drains
  // and the stale chunk has been dropped by a later streaming update.
  for (int i = 0; i < 400 && world.getChunk(0, 0); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const glm::vec3 nudge[] = {{101.5f * kChunkWidth, 80.0f, 0.5f}};
    const glm::vec3 back[] = {{100.5f * kChunkWidth, 80.0f, 0.5f}};
    world.setInterestPoints(i % 2 ? back : nudge);
  }
  CHECK(world.getChunk(0, 0) == nullptr);
}

TEST_CASE("findSpawn picks a standable column") {
  World world;
  glm::vec3 spawn = world.findSpawn();

  int cx = static_cast<int>(std::floor(spawn.x / kChunkWidth));
  int cz = static_cast<int>(std::floor(spawn.z / kChunkDepth));
  world.loadChunkSync(cx, cz);

  CHECK(world.isSolidBlock(static_cast<int>(std::floor(spawn.x)),
                           static_cast<int>(spawn.y) - 1,
                           static_cast<int>(std::floor(spawn.z))));
  CHECK(world.getBlockAt(spawn + glm::vec3(0, 0.5f, 0)) == BlockType::AIR);
  CHECK(world.getBlockAt(spawn + glm::vec3(0, 1.5f, 0)) == BlockType::AIR);
}

TEST_CASE("findSpawn stands you on the surface, never in a cave") {
  // Caves and plants (a flower on the top grass) once sent the search on
  // down the column to a cave floor.
  ScopedSeed seed;
  const uint8_t saved_gen = g_settings.worldgen;
  g_settings.worldgen = worldgen::kLatest;
  for (int i = 0; i < 12; ++i) {
    CAPTURE(i);
    g_settings.world_seed = 1000 + i;
    g_settings.noise_offset = {i * 9137.0f, i * -4211.0f};
    World world;
    glm::vec3 spawn = world.findSpawn();
    int cx = static_cast<int>(std::floor(spawn.x / kChunkWidth));
    int cz = static_cast<int>(std::floor(spawn.z / kChunkDepth));
    world.loadChunkSync(cx, cz);
    CAPTURE(spawn.y);
    for (int y = static_cast<int>(spawn.y); y < kChunkHeight; ++y) {
      BlockType b = world.getBlockAt({spawn.x, y + 0.5f, spawn.z});
      CAPTURE(y);
      REQUIRE_FALSE(blocksMovement(b)); // open to the sky
    }
  }
  g_settings.worldgen = saved_gen;
}

// ─── Remote edits ──────────────────────────────────────────────────────

TEST_CASE("an edit to a chunk that isn't loaded appears when it loads") {
  World world;
  world.applyEdit({20, 200, 5}, BlockType::GLOWSTONE); // chunk (1, 0)
  CHECK(world.getBlockAt({20.5f, 200.5f, 5.5f}) == BlockType::AIR); // unloaded
  world.loadChunkSync(1, 0);
  world.flushPendingEdits();
  CHECK(world.getBlockAt({20.5f, 200.5f, 5.5f}) == BlockType::GLOWSTONE);
}

TEST_CASE("an edit to a loaded chunk applies at once") {
  World world;
  world.loadChunkSync(0, 0);
  world.applyEdit({3, 200, 3}, BlockType::STONE);
  CHECK(world.getBlockAt({3.5f, 200.5f, 3.5f}) == BlockType::STONE);
}

TEST_CASE("a pending edit is applied even if its chunk loaded without it") {
  World world;
  world.applyEdit({3, 200, 3}, BlockType::STONE);
  // As if the chunk had been generated from the edits read just before.
  world.loadChunkSync(0, 0);
  world.setBlockAt({3, 200, 3}, BlockType::AIR);
  world.flushPendingEdits();
  CHECK(world.getBlockAt({3.5f, 200.5f, 3.5f}) == BlockType::STONE);
  world.setBlockAt({3, 200, 3}, BlockType::AIR);
  world.flushPendingEdits(); // applied once, not forever
  CHECK(world.getBlockAt({3.5f, 200.5f, 3.5f}) == BlockType::AIR);
}

TEST_CASE("a guest world built from the host's edit list matches the host") {
  World host;
  host.loadChunkSync(0, 0);
  host.loadChunkSync(-1, 0);
  host.setBlockAt({3, 200, 3}, BlockType::GLOWSTONE);
  host.setBlockAt({-5, 120, 7}, BlockType::AIR);
  host.setBlockAt({-1, 201, 15}, BlockType::STONE);

  World guest;
  guest.setLoadedEdits(std::span<const BlockEdit>(host.editList()));
  CHECK(host.editList().size() == 3);
  guest.loadChunkSync(0, 0);
  guest.loadChunkSync(-1, 0);
  guest.flushPendingEdits();
  for (int x = -16; x < 16; ++x)
    for (int z = 0; z < 16; ++z)
      for (int y = 0; y < kChunkHeight; ++y) {
        glm::vec3 p(x + 0.5f, y + 0.5f, z + 0.5f);
        if (host.getBlockAt(p) != guest.getBlockAt(p)) {
          FAIL("block differs at " << x << "," << y << "," << z);
        }
      }
}

// ─── Block light across chunk borders ──────────────────────────────────

namespace {

// Block light flooded over a whole box of loaded chunks at once: the answer
// chunk-by-chunk streaming has to reach, whatever order chunks arrive in.
struct ReferenceLight {
  int x0, z0, w, d;
  std::vector<uint8_t> light;
  uint8_t &at(int x, int y, int z) {
    return light[(x - x0) + w * ((z - z0) + d * static_cast<size_t>(y))];
  }

  ReferenceLight(const World &world, int cx0, int cz0, int chunks)
      : x0(cx0 * kChunkWidth), z0(cz0 * kChunkDepth), w(chunks * kChunkWidth),
        d(chunks * kChunkDepth), light(static_cast<size_t>(w) * d * kChunkHeight, 0) {
    auto block = [&](int x, int y, int z) {
      return world.getBlockAt(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));
    };
    std::queue<glm::ivec3> q;
    for (int y = 0; y < kChunkHeight; ++y)
      for (int z = z0; z < z0 + d; ++z)
        for (int x = x0; x < x0 + w; ++x)
          if (uint8_t e = blockLightEmission(block(x, y, z))) {
            at(x, y, z) = e;
            q.push({x, y, z});
          }
    const glm::ivec3 steps[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    while (!q.empty()) {
      glm::ivec3 p = q.front();
      q.pop();
      uint8_t l = at(p.x, p.y, p.z);
      for (glm::ivec3 s : steps) {
        glm::ivec3 n = p + s;
        if (n.x < x0 || n.x >= x0 + w || n.z < z0 || n.z >= z0 + d || n.y < 0 ||
            n.y >= kChunkHeight)
          continue;
        uint8_t op = skyLightOpacity(block(n.x, n.y, n.z));
        if (op >= 15 || l <= 1 + op) continue;
        uint8_t nl = static_cast<uint8_t>(l - 1 - op);
        if (nl > at(n.x, n.y, n.z)) {
          at(n.x, n.y, n.z) = nl;
          q.push(n);
        }
      }
    }
  }
};

void checkInnerLightMatches(World &world, int cx0, int cz0, int chunks, int margin) {
  ReferenceLight ref(world, cx0, cz0, chunks);
  long lit = 0, wrong = 0;
  for (int cx = cx0 + margin; cx < cx0 + chunks - margin; ++cx)
    for (int cz = cz0 + margin; cz < cz0 + chunks - margin; ++cz)
      for (int y = 0; y < kChunkHeight; ++y)
        for (int lz = 0; lz < kChunkDepth; ++lz)
          for (int lx = 0; lx < kChunkWidth; ++lx) {
            glm::ivec3 p{cx * kChunkWidth + lx, y, cz * kChunkDepth + lz};
            uint8_t want = ref.at(p.x, p.y, p.z);
            lit += want > 0;
            wrong += world.getBlockLightAt(p) != want;
          }
  CHECK(lit > 0);
  CHECK(wrong == 0);
}

} // namespace

TEST_CASE("a lamp's light crosses into a chunk that loads after it") {
  World world;
  // A lamp just inside chunk (0, 0)'s +x border, high in open air.
  const glm::ivec3 lamp{kChunkWidth - 2, kChunkHeight - 4, 8};
  WorldEdits edits;
  edits[{0, 0}][linearIndex(lamp.x, lamp.y, lamp.z)] = static_cast<uint8_t>(BlockType::GLOWSTONE);
  world.setLoadedEdits(std::move(edits));

  world.loadChunkSync(0, 0);
  world.loadChunkSync(1, 0); // arrives later
  CHECK(world.getBlockLightAt(lamp + glm::ivec3(3, 0, 0)) == 12);
  // And on round a corner, into the diagonal neighbour.
  world.loadChunkSync(1, 1);
  int dz = kChunkDepth - lamp.z; // to the first block of (1, 1)
  CHECK(world.getBlockLightAt(lamp + glm::ivec3(3, 0, dz)) == 15 - 3 - dz);
}

TEST_CASE("streamed-in cave light matches one flood over the whole area") {
  ScopedRenderDistance rd{0};
  Settings saved = g_settings;
  g_settings.world_seed = 12345;
  g_settings.noise_offset = noiseOffsetFor(12345);
  g_settings.worldgen = worldgen::kCaves;
  {
    // 7x7 chunks around a cave system with glow crystals, loaded in an
    // awkward order (columns, alternating ends). The inner 3x3 is more than
    // a light's reach from the edge, so it must match exactly.
    World world;
    const int cx0 = -43, cz0 = -43, n = 7;
    for (int i = 0; i < n; ++i)
      for (int j = 0; j < n; ++j) world.loadChunkSync(cx0 + i, cz0 + (i % 2 ? n - 1 - j : j));
    checkInnerLightMatches(world, cx0, cz0, n, 2);
  }
  g_settings = saved;
}

TEST_CASE("light after many edits by lights and borders matches one flood") {
  ScopedRenderDistance rd{0};
  Settings saved = g_settings;
  g_settings.world_seed = 12345;
  g_settings.noise_offset = noiseOffsetFor(12345);
  g_settings.worldgen = worldgen::kCaves;
  {
    // The cave system with glow crystals, then lamps lit, put out and walled
    // in across the inner chunks' borders and corners, in open air and in rock.
    World world;
    const int cx0 = -43, cz0 = -43, n = 7;
    for (int i = 0; i < n; ++i)
      for (int j = 0; j < n; ++j) world.loadChunkSync(cx0 + i, cz0 + j);
    const BlockType kinds[] = {BlockType::GLOWSTONE, BlockType::TORCH, BlockType::GLOW_CRYSTAL,
                               BlockType::AIR, BlockType::STONE, BlockType::WATER,
                               BlockType::GLASS};
    std::mt19937 rng(5);
    const int lo = (cx0 + 2) * kChunkWidth, span = 3 * kChunkWidth;
    for (int k = 0; k < 300; ++k) {
      int x = lo + int(rng() % span), z = (cz0 + 2) * kChunkDepth + int(rng() % span);
      if (k % 2) { // on a border
        x = (x & ~(kChunkWidth - 1)) + (rng() % 2 ? 0 : kChunkWidth - 1);
        z = (z & ~(kChunkDepth - 1)) + (rng() % 2 ? 0 : kChunkDepth - 1);
      }
      const int y = 20 + int(rng() % 120);
      world.setBlockAt({x, y, z}, kinds[rng() % std::size(kinds)]);
    }
    checkInnerLightMatches(world, cx0, cz0, n, 2);
  }
  g_settings = saved;
}

TEST_CASE("walling a lamp in darkens the chunk next door too") {
  World world;
  world.loadChunkSync(0, 0);
  world.loadChunkSync(1, 0);
  const glm::ivec3 lamp{kChunkWidth - 2, kChunkHeight - 4, 8};
  world.setBlockAt(lamp, BlockType::GLOWSTONE);
  const glm::ivec3 next_door = lamp + glm::ivec3(3, 0, 0);
  REQUIRE(world.getBlockLightAt(next_door) == 12);

  const glm::ivec3 sides[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  for (glm::ivec3 s : sides) world.setBlockAt(lamp + s, BlockType::STONE);
  CHECK(world.getBlockLightAt(next_door) == 0);
}

TEST_CASE("an edit far from any light leaves border light from neighbours in place") {
  World world;
  const glm::ivec3 lamp{kChunkWidth - 2, kChunkHeight - 4, 8};
  WorldEdits edits;
  edits[{0, 0}][linearIndex(lamp.x, lamp.y, lamp.z)] = static_cast<uint8_t>(BlockType::GLOWSTONE);
  world.setLoadedEdits(std::move(edits));
  world.loadChunkSync(0, 0);
  world.loadChunkSync(1, 0);
  const glm::ivec3 next_door = lamp + glm::ivec3(3, 0, 0);
  REQUIRE(world.getBlockLightAt(next_door) == 12);

  // Out of the lamp's reach, but in the chunk its light spills into.
  world.setBlockAt({kChunkWidth + 14, kChunkHeight - 20, 8}, BlockType::STONE);
  CHECK(world.getBlockLightAt(next_door) == 12);
}

TEST_CASE("chunks are drawn nearest the viewer first") {
  Chunk far_east(5, 0), near(0, 0), mid_north(0, -2), behind(-1, 0);
  std::vector<Chunk *> chunks{&far_east, &near, &mid_north, &behind};
  sortNearestFirst(chunks, {8.0f, 70.0f, 8.0f}); // the middle of (0, 0)
  CHECK(chunks == std::vector<Chunk *>{&near, &behind, &mid_north, &far_east});
}
