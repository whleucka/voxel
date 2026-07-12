#include "world/world.hpp"
#include "biome/biome.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "core/settings.hpp"
#include "render/renderer.hpp"
#include "world/world_save.hpp"
#include "util/lock.hpp"
#include <cmath>
#include <glm/fwd.hpp>
#include <memory>

World::World()
    : renderer(std::make_unique<Renderer>()),
      player(std::make_unique<Player>(glm::vec3(0.0f, 0.0f, 0.0f))) {}

World::~World() {
  // Stop worker threads before any members (chunks, renderer, ...) unwind —
  // an in-flight mesh job dereferences them, so they must outlive the pools.
  gen_pool.shutdown();
  mesh_pool.shutdown();
}

void World::init() {
  renderer->init();

  spiral_offsets = generateSpiralOrder(g_settings.render_distance / kChunkWidth);

  glm::vec3 pos;
  if (has_loaded_player) {
    // Restoring a saved world — use the stored position instead of searching
    // for a fresh spawn point.
    pos = loaded_player.position;
  } else {
    // Spiral outward (by chunk) using only Biome::getHeight() — pure math, no
    // chunk allocation — until we find a chunk whose center is above sea level.
    int spawn_cx = 0, spawn_cz = 0;
    constexpr int kMaxSpawnSearch = 200;
    for (int r = 0; r <= kMaxSpawnSearch; ++r) {
      bool found = false;
      for (int dx = -r; dx <= r && !found; ++dx) {
        for (int dz = -r; dz <= r && !found; ++dz) {
          if (std::abs(dx) != r && std::abs(dz) != r) continue;
          float wx = (dx + 0.5f) * kChunkWidth;
          float wz = (dz + 0.5f) * kChunkDepth;
          if (Biome::getHeight({wx, wz}) > kSeaLevel + 3) {
            spawn_cx = dx;
            spawn_cz = dz;
            found = true;
          }
        }
      }
      if (found) break;
    }

    // Generate that land chunk synchronously to scan actual block data.
    // Do NOT insert it into the chunks map — let preloadChunks handle it through
    // the normal async pipeline so its mesh gets built and uploaded properly.
    Chunk spawn_chunk(spawn_cx, spawn_cz);
    spawn_chunk.init();

    // Scan all columns for grass/dirt with 2 clear air blocks above (player is
    // 1.8 blocks tall so we need y+1 and y+2 both to be air).
    auto scanColumn = [&](int lx, int lz, auto predicate) -> int {
      for (int y = kChunkHeight - 3; y >= 1; --y) {
        BlockType surface = spawn_chunk.at(lx, y,     lz);
        BlockType above1  = spawn_chunk.at(lx, y + 1, lz);
        BlockType above2  = spawn_chunk.at(lx, y + 2, lz);
        if (predicate(surface) && above1 == BlockType::AIR && above2 == BlockType::AIR)
          return y + 1;
      }
      return -1;
    };

    auto isGrassDirt = [](BlockType b) {
      return b == BlockType::GRASS || b == BlockType::DIRT;
    };
    auto isSolid = [](BlockType b) {
      return b != BlockType::AIR && b != BlockType::WATER;
    };

    float spawn_x = (spawn_cx + 0.5f) * kChunkWidth;
    float spawn_y = -1.0f;
    float spawn_z = (spawn_cz + 0.5f) * kChunkDepth;

    // Pass 1: grass or dirt surface
    for (int lx = 0; lx < kChunkWidth && spawn_y < 0; ++lx)
      for (int lz = 0; lz < kChunkDepth && spawn_y < 0; ++lz) {
        int y = scanColumn(lx, lz, isGrassDirt);
        if (y >= 0) {
          spawn_x = spawn_cx * kChunkWidth + lx + 0.5f;
          spawn_y = static_cast<float>(y);
          spawn_z = spawn_cz * kChunkDepth + lz + 0.5f;
        }
      }

    // Pass 2: any solid non-water surface (snow, stone peaks)
    if (spawn_y < 0)
      for (int lx = 0; lx < kChunkWidth && spawn_y < 0; ++lx)
        for (int lz = 0; lz < kChunkDepth && spawn_y < 0; ++lz) {
          int y = scanColumn(lx, lz, isSolid);
          if (y >= 0) {
            spawn_x = spawn_cx * kChunkWidth + lx + 0.5f;
            spawn_y = static_cast<float>(y);
            spawn_z = spawn_cz * kChunkDepth + lz + 0.5f;
          }
        }

    pos = glm::vec3(spawn_x, spawn_y, spawn_z);
  }

  player->setPosition(pos);
  player->getCamera().setPosition(pos);

  if (has_loaded_player) {
    Camera &cam = player->getCamera();
    cam.yaw = loaded_player.yaw;
    cam.pitch = loaded_player.pitch;
    cam.processMouseMovement(0.0f, 0.0f); // refresh direction vectors
    player->setSelectedHotbarSlot(loaded_player.selected_slot);
    *player->getFlyModePtr() = (loaded_player.fly_mode != 0);
  }

  // Now compute chunk center and kick off loading
  const glm::vec3 p = player->getPosition();
  last_chunk_x = static_cast<int>(std::floor(p.x / kChunkWidth));
  last_chunk_z = static_cast<int>(std::floor(p.z / kChunkDepth));

  preloadChunks();
  updateLoadedChunks();
}

std::vector<glm::ivec2> World::generateSpiralOrder(int radius) {
  std::vector<glm::ivec2> spiral;
  spiral.reserve((2 * radius + 1) * (2 * radius + 1));

  // generate rings from center out
  spiral.emplace_back(0, 0);
  for (int r = 1; r <= radius; ++r) {
    int x = -r, z = -r;
    // Move right
    for (int i = 0; i < 2 * r; i++) {
      spiral.emplace_back(x++, z);
    }
    // Move up
    for (int i = 0; i < 2 * r; i++) {
      spiral.emplace_back(x, z++);
    }
    // Move left
    for (int i = 0; i < 2 * r; i++) {
      spiral.emplace_back(x--, z);
    }
    // Move down
    for (int i = 0; i < 2 * r; i++) {
      spiral.emplace_back(x, z--);
    }
  }

  // optional: sort exact center-out by distance
  std::sort(spiral.begin(), spiral.end(), [](auto &a, auto &b) {
    return (a.x * a.x + a.y * a.y) < (b.x * b.x + b.y * b.y);
  });

  return spiral;
}

void World::addChunk(int x, int z) {
  ChunkKey key{x, z};
  auto chunk = std::make_shared<Chunk>(x, z);

  gen_pool.enqueue([this, chunk, key]() {
    chunk->init(); // generate terrain

    // Replay any saved player edits onto this freshly generated chunk before
    // it gets meshed, then re-light so shadows/skylight reflect the changes.
    ChunkEdits ce;
    {
      std::lock_guard lk(edits_mutex);
      auto it = edits.find(key);
      if (it != edits.end()) ce = it->second;
    }
    if (!ce.empty()) {
      std::lock_guard dlk(chunk->data_mutex);
      for (const auto &[idx, block] : ce)
        chunk->setBlockLinear(idx, static_cast<BlockType>(block));
      chunk->computeSkyLight();
      chunk->computeBlockLight();
    }

    // Once generation is done, queue for meshing
    mesh_pool.enqueue([this, chunk]() {
      chunk->buildMeshData(this, renderer->getTextureManager());
      {
        WriteLock lock(chunks_mutex);
        upload_queue.push(chunk);
      }
    });

    // Safely add the chunk to the world (even before meshing)
    {
      WriteLock lock(chunks_mutex);
      chunks.emplace(key, chunk);
    }

    // If this chunk carries emitters (replayed torches), record that so the
    // relight machinery activates.
    if (chunk->hasEmitters())
      emitters_exist.store(true, std::memory_order_relaxed);

    // If torches sit near this freshly streamed-in chunk, queue a cross-chunk
    // relight (handled on the main thread) so light bleeds across the new border.
    if (anyEmitterInRegion(key.x, key.z, 2)) {
      std::lock_guard lk(relight_mutex);
      relight_requests.push_back({key.x, key.z});
    }
  });
}

void World::update(float dt) {
  player->update(dt, this);
  cloud_time += dt;

  // Drain cross-chunk relight requests queued as chunks stream in near torches.
  if (emitters_exist.load(std::memory_order_relaxed)) {
    std::vector<glm::ivec2> reqs;
    {
      std::lock_guard lk(relight_mutex);
      reqs.swap(relight_requests);
    }
    if (!reqs.empty()) {
      std::sort(reqs.begin(), reqs.end(),
                [](auto &a, auto &b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
      reqs.erase(std::unique(reqs.begin(), reqs.end(),
                             [](auto &a, auto &b) { return a.x == b.x && a.y == b.y; }),
                 reqs.end());
      for (const auto &r : reqs) relightBlockRegion(r.x, r.y);
    }
  }

  glm::vec3 pos = player->getPosition();
  int current_chunk_x = static_cast<int>(floor(pos.x / kChunkWidth));
  int current_chunk_z = static_cast<int>(floor(pos.z / kChunkDepth));

  if (current_chunk_x != last_chunk_x || current_chunk_z != last_chunk_z) {
    last_chunk_x = current_chunk_x;
    last_chunk_z = current_chunk_z;
    updateLoadedChunks();
  }

  int uploads_this_frame = 0;
  {
    WriteLock lock(chunks_mutex);
    while (!upload_queue.empty() && uploads_this_frame < kMaxUploadsPerFrame) {
      upload_queue.front()->uploadGPU();
      upload_queue.pop();
      ++uploads_this_frame;
    }
  }
}

void World::preloadChunks() {
  int chunksAdded = 0;

  for (const auto &offset : spiral_offsets) {
    if (offset.x * offset.x + offset.y * offset.y >
        kChunkPreloadRadius * kChunkPreloadRadius)
      continue;

    int cx = last_chunk_x + offset.x;
    int cz = last_chunk_z + offset.y;

    if (!getChunk(cx, cz)) {
      addChunk(cx, cz);
      ++chunksAdded;
    }
  }
}

void World::updateLoadedChunks() {
  const int r = g_settings.render_distance / kChunkWidth;
  constexpr size_t MAX_ALLOWED_CHUNKS = 3000; // tweak as needed

  // Prevent runaway growth
  {
    ReadLock lock(chunks_mutex);
    if (chunks.size() > MAX_ALLOWED_CHUNKS) {
      return;
    }
  }

  robin_hood::unordered_set<ChunkKey, ChunkKeyHash> keep;
  keep.reserve((2 * r + 1) * (2 * r + 1));

  int chunksAdded = 0;

  for (const auto &offset : spiral_offsets) {
    int cx = last_chunk_x + offset.x;
    int cz = last_chunk_z + offset.y;

    if (offset.x * offset.x + offset.y * offset.y > r * r)
      continue;

    keep.insert({cx, cz});

    if (!getChunk(cx, cz) && chunksAdded < kMaxChunksPerFrame) {
      addChunk(cx, cz);
      ++chunksAdded;
    }
  }

  // Unload chunks outside radius
  std::vector<ChunkKey> to_remove;
  {
    WriteLock lock(chunks_mutex);
    for (auto &[key, _] : chunks)
      if (!keep.count(key))
        to_remove.push_back(key);
    for (auto &key : to_remove)
      chunks.erase(key);
  }
}

void World::unloadChunk(int x, int z) {
  WriteLock lock(chunks_mutex);
  chunks.erase({x, z});
}

void World::rebuildChunk(int cx, int cz) {
  auto chunk = getChunk(cx, cz);
  if (!chunk) return;
  mesh_pool.enqueue([this, chunk]() {
    chunk->buildMeshData(this, renderer->getTextureManager());
    WriteLock lock(chunks_mutex);
    upload_queue.push(chunk);
  });
}

bool World::anyEmitterInRegion(int ccx, int ccz, int radius) const {
  if (!emitters_exist.load(std::memory_order_relaxed)) return false;
  for (int cx = ccx - radius; cx <= ccx + radius; ++cx)
    for (int cz = ccz - radius; cz <= ccz + radius; ++cz) {
      auto c = getChunk(cx, cz);
      if (c && c->hasEmitters()) return true;
    }
  return false;
}

// Recompute block light over a 5x5 chunk box (so torches up to 2 chunks away
// are accounted for) into a temp buffer, then persist the inner 3x3 and remesh
// it. Clearing + reflooding makes both placement and removal correct, and light
// crosses chunk borders. Runs on the main thread.
void World::relightBlockRegion(int ccx, int ccz) {
  constexpr int KEEP = 1;    // chunks we write back
  constexpr int COMPUTE = 2; // chunks we flood
  constexpr int SPAN = 2 * COMPUTE + 1;
  const int W = SPAN * kChunkWidth;
  const int D = SPAN * kChunkDepth;
  const int H = kChunkHeight;

  // Snapshot the box's chunks once (nulls = unloaded → treated as opaque).
  std::shared_ptr<Chunk> grid[SPAN][SPAN];
  for (int i = 0; i < SPAN; ++i)
    for (int j = 0; j < SPAN; ++j)
      grid[i][j] = getChunk(ccx - COMPUTE + i, ccz - COMPUTE + j);

  auto lidx = [&](int bx, int y, int bz) {
    return static_cast<size_t>(bx) +
           W * (static_cast<size_t>(bz) + static_cast<size_t>(D) * y);
  };
  std::vector<uint8_t> light(static_cast<size_t>(W) * D * H, 0);

  // Opacity of a box-local voxel (box coords are always >= 0).
  auto opacityAt = [&](int bx, int y, int bz) -> uint8_t {
    if (y < 0 || y >= H) return 15;
    const auto &c = grid[bx / kChunkWidth][bz / kChunkDepth];
    if (!c) return 15;
    return skyLightOpacity(c->at(bx % kChunkWidth, y, bz % kChunkDepth));
  };

  // Seed from every emitter in the box.
  std::queue<std::tuple<int, int, int>> q;
  for (int i = 0; i < SPAN; ++i)
    for (int j = 0; j < SPAN; ++j) {
      const auto &c = grid[i][j];
      if (!c || !c->hasEmitters()) continue;
      for (int y = 0; y < H; ++y)
        for (int lz = 0; lz < kChunkDepth; ++lz)
          for (int lx = 0; lx < kChunkWidth; ++lx) {
            uint8_t emit = blockLightEmission(c->at(lx, y, lz));
            if (!emit) continue;
            int bx = i * kChunkWidth + lx;
            int bz = j * kChunkDepth + lz;
            size_t k = lidx(bx, y, bz);
            if (emit > light[k]) { light[k] = emit; q.push({bx, y, bz}); }
          }
    }

  // Flood fill within the box.
  static constexpr int DX[6] = {-1, 1, 0, 0, 0, 0};
  static constexpr int DY[6] = {0, 0, -1, 1, 0, 0};
  static constexpr int DZ[6] = {0, 0, 0, 0, -1, 1};
  while (!q.empty()) {
    auto [bx, by, bz] = q.front();
    q.pop();
    uint8_t l = light[lidx(bx, by, bz)];
    if (l <= 1) continue;
    for (int d = 0; d < 6; ++d) {
      int nx = bx + DX[d], ny = by + DY[d], nz = bz + DZ[d];
      if (nx < 0 || nx >= W || ny < 0 || ny >= H || nz < 0 || nz >= D) continue;
      uint8_t op = opacityAt(nx, ny, nz);
      if (op >= 15) continue;
      uint8_t cost = 1 + op;
      uint8_t nl = (l > cost) ? static_cast<uint8_t>(l - cost) : 0;
      if (nl == 0) continue;
      size_t k = lidx(nx, ny, nz);
      if (nl > light[k]) { light[k] = nl; q.push({nx, ny, nz}); }
    }
  }

  // Persist the inner KEEP region back into the chunks, then remesh them.
  for (int i = COMPUTE - KEEP; i <= COMPUTE + KEEP; ++i)
    for (int j = COMPUTE - KEEP; j <= COMPUTE + KEEP; ++j) {
      const auto &c = grid[i][j];
      if (!c) continue;
      {
        std::lock_guard lock(c->data_mutex);
        for (int y = 0; y < H; ++y)
          for (int lz = 0; lz < kChunkDepth; ++lz)
            for (int lx = 0; lx < kChunkWidth; ++lx)
              c->setBlockLightLinear(
                  lx + kChunkWidth * (lz + kChunkDepth * y),
                  light[lidx(i * kChunkWidth + lx, y, j * kChunkDepth + lz)]);
      }
      rebuildChunk(ccx - COMPUTE + i, ccz - COMPUTE + j);
    }
}

void World::setBlockAt(const glm::ivec3& worldPos, BlockType type) {
  int bx = worldPos.x, by = worldPos.y, bz = worldPos.z;
  if (by < 0 || by >= kChunkHeight) return;

  int cx = static_cast<int>(std::floor(static_cast<float>(bx) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(bz) / kChunkDepth));

  auto chunk = getChunk(cx, cz);
  if (!chunk) return;

  int lx = bx - cx * kChunkWidth;
  int lz = bz - cz * kChunkDepth;

  BlockType oldType;
  {
    std::lock_guard lock(chunk->data_mutex);
    oldType = chunk->at(lx, by, lz);
    chunk->at(lx, by, lz) = type;
    chunk->computeSkyLight(); // re-propagate sky light after block change
    // Keep the emitter count in sync for this single-block change.
    chunk->adjustEmitterCount((blockLightEmission(type) > 0 ? 1 : 0) -
                              (blockLightEmission(oldType) > 0 ? 1 : 0));
  }
  if (blockLightEmission(type) > 0)
    emitters_exist.store(true, std::memory_order_relaxed);

  // Record the delta so it survives save/load (breaking a block records AIR).
  {
    uint32_t idx = lx + kChunkWidth * (lz + kChunkDepth * by);
    std::lock_guard lk(edits_mutex);
    edits[{cx, cz}][idx] = static_cast<uint8_t>(type);
  }

  // Block light: if a torch is involved or nearby, run the cross-chunk relight
  // (recomputes + remeshes the 3x3 around the edit). Otherwise take the cheap
  // intra-chunk path and rebuild the edited chunk (+ edge neighbors).
  bool emitterInvolved =
      blockLightEmission(type) > 0 || blockLightEmission(oldType) > 0;
  if (emitterInvolved || anyEmitterInRegion(cx, cz, 2)) {
    relightBlockRegion(cx, cz);
  } else {
    {
      std::lock_guard lock(chunk->data_mutex);
      chunk->computeBlockLight();
    }
    rebuildChunk(cx, cz);
    if (lx == 0)              rebuildChunk(cx - 1, cz);
    if (lx == kChunkWidth-1)  rebuildChunk(cx + 1, cz);
    if (lz == 0)              rebuildChunk(cx, cz - 1);
    if (lz == kChunkDepth-1)  rebuildChunk(cx, cz + 1);
  }
}

bool World::isChunkInFrustum(const glm::vec4 planes[6], const glm::vec3 &min,
                             const glm::vec3 &max) {
  for (int i = 0; i < 6; i++) {
    const glm::vec3 n = glm::vec3(planes[i]);
    const float d = planes[i].w;

    // pick vertex farthest *in the direction of the plane normal*
    glm::vec3 positive = {(n.x > 0 ? max.x : min.x), (n.y > 0 ? max.y : min.y),
                          (n.z > 0 ? max.z : min.z)};

    // plane eq: n·x + d > 0 means in front
    if (glm::dot(n, positive) + d < 0)
      return false; // entire box is behind this plane
  }
  return true;
}

void World::render(glm::mat4 &view, glm::mat4 &projection, float timeOfDay,
                   int viewportWidth, int viewportHeight) {
  ReadLock lock(chunks_mutex);

  glm::mat4 viewProj = projection * view;

  glm::vec4 planes[6];
  player->getCamera().getFrustumPlanes(planes, viewProj);

  std::vector<Chunk *> visibleChunks;
  visibleChunks.reserve(chunks.size());

  for (const auto &[key, chunk] : chunks) {
    if (!chunk) continue;
    glm::vec3 min = {key.x * kChunkWidth, 0.0f, key.z * kChunkDepth};
    glm::vec3 max = {(key.x + 1) * kChunkWidth,
                     static_cast<float>(kChunkHeight),
                     (key.z + 1) * kChunkDepth};

    if (isChunkInFrustum(planes, min, max)) {
      visibleChunks.push_back(chunk.get());
    }
  }

  // Shadow pass: render depth from sun's perspective (uses all loaded chunks
  // so that off-screen geometry can still cast shadows into view)
  renderer->shadowPass(chunks, player->getPosition(), timeOfDay,
                       viewportWidth, viewportHeight);

  // Sky must be drawn before world geometry (it uses GL_LEQUAL depth and writes
  // no depth values, so any chunk fragment will correctly overwrite it).
  renderer->drawSky(view, projection, timeOfDay);

  renderer->drawChunks(visibleChunks, view, projection,
                       player->getPosition(), player->isUnderwater(), timeOfDay);

  // Clouds drawn after opaque & transparent terrain so they blend correctly
  // with the sky behind them while terrain in front occludes them via depth.
  renderer->drawClouds(view, projection, player->getPosition(), timeOfDay,
                       cloud_time);
}

std::shared_ptr<Chunk> World::getChunk(int x, int z) {
  return std::const_pointer_cast<Chunk>(std::as_const(*this).getChunk(x, z));
}

std::shared_ptr<const Chunk> World::getChunk(int x, int z) const {
  ReadLock lock(chunks_mutex);
  auto it = chunks.find({x, z});
  if (it == chunks.end())
    return nullptr;
  return it->second;
}

size_t World::getChunkCount() const {
  ReadLock lock(chunks_mutex);
  return chunks.size();
}

BlockType World::getBlockAt(const glm::vec3& worldPos) const {
  int bx = static_cast<int>(std::floor(worldPos.x));
  int by = static_cast<int>(std::floor(worldPos.y));
  int bz = static_cast<int>(std::floor(worldPos.z));

  if (by < 0 || by >= kChunkHeight) {
    return BlockType::AIR;
  }

  int cx = static_cast<int>(std::floor(static_cast<float>(bx) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(bz) / kChunkDepth));

  auto chunk = getChunk(cx, cz);
  if (!chunk) {
    return BlockType::AIR;
  }

  int lx = bx - cx * kChunkWidth;
  int lz = bz - cz * kChunkDepth;

  return chunk->safeAt(lx, by, lz);
}

bool World::isSolidBlock(int bx, int by, int bz) const {
  if (by < 0 || by >= kChunkHeight) return false;
  BlockType block = getBlockAt(glm::vec3(bx + 0.5f, by + 0.5f, bz + 0.5f));
  return block != BlockType::AIR && !isLiquid(block);
}

uint8_t World::getSkyLightAt(const glm::ivec3 &p) const {
  if (p.y < 0 || p.y >= kChunkHeight) return 15;
  int cx = static_cast<int>(std::floor(static_cast<float>(p.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(p.z) / kChunkDepth));
  auto chunk = getChunk(cx, cz);
  if (!chunk) return 0;
  return chunk->safeSkyLight(p.x - cx * kChunkWidth, p.y, p.z - cz * kChunkDepth);
}

uint8_t World::getBlockLightAt(const glm::ivec3 &p) const {
  if (p.y < 0 || p.y >= kChunkHeight) return 0;
  int cx = static_cast<int>(std::floor(static_cast<float>(p.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(p.z) / kChunkDepth));
  auto chunk = getChunk(cx, cz);
  if (!chunk) return 0;
  return chunk->safeBlockLight(p.x - cx * kChunkWidth, p.y, p.z - cz * kChunkDepth);
}

// ─── DDA Raycast ───────────────────────────────────────────────────────
// Steps through the voxel grid one cell at a time along a ray.
// Based on "A Fast Voxel Traversal Algorithm" (Amanatides & Woo, 1987).

RaycastResult World::raycast(const glm::vec3& origin, const glm::vec3& direction,
                              float max_distance) const {
  RaycastResult result;

  if (glm::length(direction) < 1e-8f) return result;

  glm::vec3 dir = glm::normalize(direction);

  // Blocks occupy [x, x+1] in world space (standard voxel layout).
  // No shift needed — DDA cell boundaries align directly with block faces.
  const glm::vec3& o = origin;

  // Current voxel position
  int x = static_cast<int>(std::floor(o.x));
  int y = static_cast<int>(std::floor(o.y));
  int z = static_cast<int>(std::floor(o.z));

  // Step direction for each axis (+1 or -1)
  int stepX = (dir.x >= 0) ? 1 : -1;
  int stepY = (dir.y >= 0) ? 1 : -1;
  int stepZ = (dir.z >= 0) ? 1 : -1;

  // tMax: distance along ray to next voxel boundary for each axis
  // tDelta: distance along ray to cross one full voxel for each axis
  float tMaxX, tMaxY, tMaxZ;
  float tDeltaX, tDeltaY, tDeltaZ;

  if (std::abs(dir.x) > 1e-8f) {
    tDeltaX = std::abs(1.0f / dir.x);
    tMaxX = ((stepX > 0) ? (std::floor(o.x) + 1.0f - o.x)
                          : (o.x - std::floor(o.x))) * tDeltaX;
  } else {
    tDeltaX = 1e30f;
    tMaxX = 1e30f;
  }

  if (std::abs(dir.y) > 1e-8f) {
    tDeltaY = std::abs(1.0f / dir.y);
    tMaxY = ((stepY > 0) ? (std::floor(o.y) + 1.0f - o.y)
                          : (o.y - std::floor(o.y))) * tDeltaY;
  } else {
    tDeltaY = 1e30f;
    tMaxY = 1e30f;
  }

  if (std::abs(dir.z) > 1e-8f) {
    tDeltaZ = std::abs(1.0f / dir.z);
    tMaxZ = ((stepZ > 0) ? (std::floor(o.z) + 1.0f - o.z)
                          : (o.z - std::floor(o.z))) * tDeltaZ;
  } else {
    tDeltaZ = 1e30f;
    tMaxZ = 1e30f;
  }

  float distance = 0.0f;

  // Maximum iterations to prevent infinite loops
  int max_steps = static_cast<int>(max_distance * 3.0f) + 1;

  for (int i = 0; i < max_steps; ++i) {
    // Check current voxel (skip the very first one if player is inside it)
    BlockType block = getBlockAt(glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f));

    if (block != BlockType::AIR && !isLiquid(block)) {
      result.hit = true;
      result.block_pos = glm::ivec3(x, y, z);
      result.distance = distance;
      result.block_type = block;
      return result;
    }

    // Advance to next voxel boundary (step along the axis with smallest tMax)
    if (tMaxX < tMaxY) {
      if (tMaxX < tMaxZ) {
        distance = tMaxX;
        if (distance > max_distance) break;
        x += stepX;
        result.normal = glm::ivec3(-stepX, 0, 0);
        tMaxX += tDeltaX;
      } else {
        distance = tMaxZ;
        if (distance > max_distance) break;
        z += stepZ;
        result.normal = glm::ivec3(0, 0, -stepZ);
        tMaxZ += tDeltaZ;
      }
    } else {
      if (tMaxY < tMaxZ) {
        distance = tMaxY;
        if (distance > max_distance) break;
        y += stepY;
        result.normal = glm::ivec3(0, -stepY, 0);
        tMaxY += tDeltaY;
      } else {
        distance = tMaxZ;
        if (distance > max_distance) break;
        z += stepZ;
        result.normal = glm::ivec3(0, 0, -stepZ);
        tMaxZ += tDeltaZ;
      }
    }
  }

  return result; // no hit
}

// ─── Persistence ─────────────────────────────────────────────────────────────

void World::setLoadedEdits(WorldEdits e) {
  std::lock_guard lk(edits_mutex);
  edits = std::move(e);
}

void World::setLoadedPlayer(const WorldSave::PlayerData &p) {
  loaded_player = p;
  has_loaded_player = true;
}

WorldEdits World::snapshotEdits() const {
  std::lock_guard lk(edits_mutex);
  return edits;
}
