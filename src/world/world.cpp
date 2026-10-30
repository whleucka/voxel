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
    : spiral_offsets(
          generateSpiralOrder(g_settings.render_distance / kChunkWidth)),
      renderer(std::make_unique<Renderer>()) {}

World::~World() {
  // Stop worker threads before any members (chunks, renderer, ...) unwind —
  // an in-flight mesh job dereferences them, so they must outlive the pools.
  gen_pool.shutdown();
  mesh_pool.shutdown();
}

void World::init() {
  renderer->init();
}

// A fresh spawn point: the first land chunk found spiralling out from the
// origin, then a grass/dirt (or failing that, any solid) column with headroom.
glm::vec3 World::findSpawn() const {
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

  // Each column's surface: its topmost block that you can't walk through
  // (plants on top don't count). A spot there needs two passable blocks above
  // (the player is 1.8 tall). Only the surface is looked at: going on down a
  // column whose top doesn't suit finds cave floors.
  auto scanColumn = [&](int lx, int lz, auto predicate) -> int {
    for (int y = kChunkHeight - 3; y >= 1; --y) {
      BlockType surface = spawn_chunk.at(lx, y, lz);
      if (!blocksMovement(surface) && !isLiquid(surface)) continue;
      if (predicate(surface) && isPassable(spawn_chunk.at(lx, y + 1, lz)) &&
          isPassable(spawn_chunk.at(lx, y + 2, lz)))
        return y + 1;
      return -1;
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

  return glm::vec3(spawn_x, spawn_y, spawn_z);
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

// Generate terrain, then replay any saved player edits onto the fresh chunk
// before it gets meshed, re-lighting so shadows/skylight reflect the changes.
void World::populateChunk(Chunk &chunk, const ChunkKey &key) {
  chunk.init();

  ChunkEdits ce;
  {
    std::lock_guard lk(edits_mutex);
    auto it = edits.find(key);
    if (it != edits.end()) ce = it->second;
  }
  if (!ce.empty()) {
    std::lock_guard dlk(chunk.data_mutex);
    for (const auto &[idx, block] : ce)
      chunk.setBlockLinear(idx, static_cast<BlockType>(block));
    chunk.computeSkyLight();
    chunk.computeBlockLight();
  }
}

void World::loadChunkSync(int x, int z) {
  ChunkKey key{x, z};
  auto chunk = std::make_shared<Chunk>(x, z);
  populateChunk(*chunk, key);
  {
    WriteLock lock(chunks_mutex);
    chunks.insert_or_assign(key, chunk);
  }
  spreadBorderLight(key);
}

void World::addChunk(int x, int z) {
  ChunkKey key{x, z};
  auto chunk = std::make_shared<Chunk>(x, z);

  gen_pool.enqueue([this, chunk, key]() {
    populateChunk(*chunk, key);
    {
      WriteLock lock(chunks_mutex);
      chunks.emplace(key, chunk);
    }

    // Light from loaded neighbours shines in (and this chunk's out) before it
    // is meshed; neighbours that brighten are remeshed.
    std::vector<ChunkKey> brighter = spreadBorderLight(key);

    mesh_pool.enqueue([this, chunk]() {
      chunk->buildMeshData(this, renderer->getTextureManager());
      WriteLock lock(chunks_mutex);
      upload_queue.push(chunk);
    });
    for (const ChunkKey &k : brighter) rebuildChunk(k.x, k.z);
  });
}

void World::update(float dt) {
  cloud_time += dt;
  flushPendingEdits();

  int uploads_this_frame = 0;
  {
    WriteLock lock(chunks_mutex);
    while (!upload_queue.empty() && uploads_this_frame < kMaxUploadsPerFrame) {
      upload_queue.front()->uploadGPU();
      glm::ivec2 pos = upload_queue.front()->getPos();
      renderer->shadowCasterChanged({pos.x, pos.y});
      upload_queue.pop();
      ++uploads_this_frame;
    }
  }
}

void World::setInterestPoints(std::span<const glm::vec3> points) {
  std::vector<glm::ivec2> centers;
  centers.reserve(points.size());
  for (const glm::vec3 &p : points)
    centers.emplace_back(static_cast<int>(std::floor(p.x / kChunkWidth)),
                         static_cast<int>(std::floor(p.z / kChunkDepth)));

  // Streaming only reacts to chunk-boundary crossings, as it always has.
  if (centers == interest_chunks) return;

  bool first = interest_chunks.empty();
  interest_chunks = std::move(centers);
  if (first) preloadChunks();
  updateLoadedChunks();
}

void World::preloadChunks() {
  for (const glm::ivec2 &center : interest_chunks) {
    for (const auto &offset : spiral_offsets) {
      if (offset.x * offset.x + offset.y * offset.y >
          kChunkPreloadRadius * kChunkPreloadRadius)
        continue;

      int cx = center.x + offset.x;
      int cz = center.y + offset.y;

      if (!getChunk(cx, cz)) addChunk(cx, cz);
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
  keep.reserve(interest_chunks.size() * (2 * r + 1) * (2 * r + 1));

  // The per-frame chunk budget is shared by all interest points.
  int chunksAdded = 0;

  for (const glm::ivec2 &center : interest_chunks) {
    for (const auto &offset : spiral_offsets) {
      int cx = center.x + offset.x;
      int cz = center.y + offset.y;

      if (offset.x * offset.x + offset.y * offset.y > r * r)
        continue;

      keep.insert({cx, cz});

      if (!getChunk(cx, cz) && chunksAdded < kMaxChunksPerFrame) {
        addChunk(cx, cz);
        ++chunksAdded;
      }
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
  for (const ChunkKey &key : to_remove) renderer->shadowCasterChanged(key);
}

void World::unloadChunk(int x, int z) {
  {
    WriteLock lock(chunks_mutex);
    chunks.erase({x, z});
  }
  renderer->shadowCasterChanged({x, z});
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

std::vector<ChunkKey> World::spreadBorderLight(const ChunkKey &origin) {
  std::vector<ChunkKey> changed;
  auto self = getChunk(origin.x, origin.z);
  if (!self) return changed;

  // Moves light across one border. Only one chunk is locked at a time, so two
  // workers spreading between the same chunks can't deadlock.
  auto shine = [](Chunk &from, Chunk &to, int to_face) {
    Chunk::BorderLight light;
    {
      std::lock_guard lock(from.data_mutex);
      light = from.borderLight(to_face ^ 1);
    }
    std::lock_guard lock(to.data_mutex);
    return to.acceptBorderLight(to_face, light);
  };
  auto neighbour = [&](const ChunkKey &k, int face) {
    glm::ivec2 step = Chunk::kFaceStep[face];
    return ChunkKey{k.x + step.x, k.z + step.y};
  };

  // What the neighbours already shine at the origin...
  for (int face = 0; face < 4; ++face) {
    ChunkKey nk = neighbour(origin, face);
    if (auto n = getChunk(nk.x, nk.z)) shine(*n, *self, face);
  }
  // ...then outward from every chunk with something new to give. Light only
  // rises and is bounded, so this ends; in practice it stops within a chunk
  // or two, since light fades by one a block.
  std::vector<ChunkKey> work{origin};
  while (!work.empty()) {
    ChunkKey k = work.back();
    work.pop_back();
    auto c = getChunk(k.x, k.z);
    if (!c) continue;
    for (int face = 0; face < 4; ++face) {
      ChunkKey nk = neighbour(k, face);
      auto n = getChunk(nk.x, nk.z);
      if (!n || !shine(*c, *n, face ^ 1)) continue;
      work.push_back(nk);
      if (!(nk == origin) && std::find(changed.begin(), changed.end(), nk) == changed.end())
        changed.push_back(nk);
    }
  }
  return changed;
}

bool World::emitterInReach(const glm::ivec3 &pos) const {
  constexpr int kReach = 14; // the brightest light (15) fades out within this
  const int y0 = std::max(0, pos.y - kReach), y1 = std::min(kChunkHeight - 1, pos.y + kReach);
  const int cx0 = static_cast<int>(std::floor(float(pos.x - kReach) / kChunkWidth));
  const int cx1 = static_cast<int>(std::floor(float(pos.x + kReach) / kChunkWidth));
  const int cz0 = static_cast<int>(std::floor(float(pos.z - kReach) / kChunkDepth));
  const int cz1 = static_cast<int>(std::floor(float(pos.z + kReach) / kChunkDepth));
  for (int cx = cx0; cx <= cx1; ++cx)
    for (int cz = cz0; cz <= cz1; ++cz) {
      auto c = getChunk(cx, cz);
      if (!c || !c->hasEmitters()) continue;
      const int bx = cx * kChunkWidth, bz = cz * kChunkDepth;
      const int x0 = std::max(0, pos.x - kReach - bx), x1 = std::min(kChunkWidth - 1, pos.x + kReach - bx);
      const int z0 = std::max(0, pos.z - kReach - bz), z1 = std::min(kChunkDepth - 1, pos.z + kReach - bz);
      for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
          for (int x = x0; x <= x1; ++x) {
            uint8_t emit = blockLightEmission(c->at(x, y, z));
            int d = std::abs(bx + x - pos.x) + std::abs(y - pos.y) + std::abs(bz + z - pos.z);
            if (emit > d) return true;
          }
    }
  return false;
}

// Recompute block light around an edit at `pos`. Light only changes within
// kReach of the edit (the brightest light fades out by then), and light there
// comes only from emitters within kReach of it, so flooding a box kReach * 2
// out from the edit and keeping the inner kReach is exact. Clearing and
// reflooding makes placement and removal both correct, and light crosses chunk
// borders. Runs on the main thread.
void World::relightBlockRegion(const glm::ivec3 &pos) {
  constexpr int kReach = 14;           // the brightest light (15) fades out within this
  constexpr int kMargin = 2 * kReach;  // emitters this far out can shine into it
  constexpr int kSide = 2 * kMargin + 1;
  const glm::ivec3 lo{pos.x - kMargin, std::max(0, pos.y - kMargin), pos.z - kMargin};
  const int H = std::min(kChunkHeight - 1, pos.y + kMargin) - lo.y + 1;

  auto floorDiv = [](int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
  const int cx0 = floorDiv(lo.x, kChunkWidth), cz0 = floorDiv(lo.z, kChunkDepth);
  constexpr int kGrid = kSide / kChunkWidth + 2; // chunks the box can touch, per side
  static_assert(kChunkWidth == kChunkDepth);
  std::shared_ptr<Chunk> grid[kGrid][kGrid];
  for (int i = 0; i < kGrid; ++i)
    for (int j = 0; j < kGrid; ++j) grid[i][j] = getChunk(cx0 + i, cz0 + j);

  auto lidx = [&](int bx, int by, int bz) {
    return static_cast<size_t>(bx) + kSide * (static_cast<size_t>(bz) + kSide * by);
  };
  const size_t cells = static_cast<size_t>(kSide) * kSide * H;
  std::vector<uint8_t> light(cells, 0);
  std::vector<uint8_t> opacity(cells, 15); // unloaded chunks are opaque
  std::vector<uint32_t> queue;

  // Each loaded chunk's part of the box: its opacity, and its emitters as seeds.
  auto overlap = [&](int i, int j, auto &&visit) {
    const int bx = (cx0 + i) * kChunkWidth, bz = (cz0 + j) * kChunkDepth;
    const int x0 = std::max(lo.x, bx), x1 = std::min(lo.x + kSide, bx + kChunkWidth) - 1;
    const int z0 = std::max(lo.z, bz), z1 = std::min(lo.z + kSide, bz + kChunkDepth) - 1;
    for (int y = 0; y < H; ++y)
      for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x)
          visit(x - bx, lo.y + y, z - bz, lidx(x - lo.x, y, z - lo.z));
  };
  for (int i = 0; i < kGrid; ++i)
    for (int j = 0; j < kGrid; ++j) {
      const auto &c = grid[i][j];
      if (!c) continue;
      const bool emitters = c->hasEmitters();
      const BlockType *blocks = c->blockData();
      overlap(i, j, [&](int lx, int y, int lz, size_t k) {
        BlockType b = blocks[lx + kChunkWidth * (lz + kChunkDepth * y)];
        opacity[k] = skyLightOpacity(b);
        if (!emitters) return;
        if (uint8_t emit = blockLightEmission(b)) {
          light[k] = emit;
          queue.push_back(static_cast<uint32_t>(k));
        }
      });
    }

  // Flood fill within the box.
  const long step[6] = {-1, 1, -long(kSide) * kSide, long(kSide) * kSide, -kSide, kSide};
  for (size_t head = 0; head < queue.size(); ++head) {
    const size_t k = queue[head];
    const uint8_t l = light[k];
    if (l <= 1) continue;
    const int bx = static_cast<int>(k % kSide);
    const int bz = static_cast<int>(k / kSide % kSide);
    const int by = static_cast<int>(k / (static_cast<size_t>(kSide) * kSide));
    const bool open[6] = {bx > 0, bx < kSide - 1, by > 0, by < H - 1, bz > 0, bz < kSide - 1};
    for (int d = 0; d < 6; ++d) {
      if (!open[d]) continue;
      const size_t n = k + step[d];
      const uint8_t op = opacity[n];
      if (op >= 15) continue;
      const uint8_t cost = 1 + op;
      if (l <= cost) continue;
      const uint8_t nl = static_cast<uint8_t>(l - cost);
      if (nl > light[n]) { light[n] = nl; queue.push_back(static_cast<uint32_t>(n)); }
    }
  }

  // Persist the inner kReach back into the chunks.
  const glm::ivec3 keep_lo{pos.x - kReach, std::max(0, pos.y - kReach), pos.z - kReach};
  const glm::ivec3 keep_hi{pos.x + kReach, std::min(kChunkHeight - 1, pos.y + kReach),
                           pos.z + kReach};
  for (int i = 0; i < kGrid; ++i)
    for (int j = 0; j < kGrid; ++j) {
      const auto &c = grid[i][j];
      if (!c) continue;
      const int bx = (cx0 + i) * kChunkWidth, bz = (cz0 + j) * kChunkDepth;
      const int x0 = std::max(keep_lo.x, bx), x1 = std::min(keep_hi.x, bx + kChunkWidth - 1);
      const int z0 = std::max(keep_lo.z, bz), z1 = std::min(keep_hi.z, bz + kChunkDepth - 1);
      if (x0 > x1 || z0 > z1) continue;
      std::lock_guard lock(c->data_mutex);
      for (int y = keep_lo.y; y <= keep_hi.y; ++y)
        for (int z = z0; z <= z1; ++z)
          for (int x = x0; x <= x1; ++x)
            c->setBlockLightLinear((x - bx) + kChunkWidth * ((z - bz) + kChunkDepth * y),
                                   light[lidx(x - lo.x, y - lo.y, z - lo.z)]);
    }

  // Remesh every chunk that holds changed light or borders on it.
  for (int cx = floorDiv(keep_lo.x - 1, kChunkWidth); cx <= floorDiv(keep_hi.x + 1, kChunkWidth); ++cx)
    for (int cz = floorDiv(keep_lo.z - 1, kChunkDepth); cz <= floorDiv(keep_hi.z + 1, kChunkDepth); ++cz)
      rebuildChunk(cx, cz);
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

  // Record the delta so it survives save/load (breaking a block records AIR).
  {
    uint32_t idx = lx + kChunkWidth * (lz + kChunkDepth * by);
    std::lock_guard lk(edits_mutex);
    edits[{cx, cz}][idx] = static_cast<uint8_t>(type);
  }

  // Block light: if a torch is involved or close enough to shine here, run the
  // cross-chunk relight (recomputes + remeshes around the edit), since
  // light may have to go down. Otherwise no light changes here: recompute the
  // chunk, take back what its neighbours shine in, and rebuild the edited
  // chunk (+ edge neighbors).
  bool emitterInvolved =
      blockLightEmission(type) > 0 || blockLightEmission(oldType) > 0;
  if (emitterInvolved || emitterInReach(worldPos)) {
    relightBlockRegion(worldPos);
  } else {
    {
      std::lock_guard lock(chunk->data_mutex);
      chunk->computeBlockLight();
    }
    for (const ChunkKey &k : spreadBorderLight({cx, cz})) rebuildChunk(k.x, k.z);
    rebuildChunk(cx, cz);
    if (lx == 0)              rebuildChunk(cx - 1, cz);
    if (lx == kChunkWidth-1)  rebuildChunk(cx + 1, cz);
    if (lz == 0)              rebuildChunk(cx, cz - 1);
    if (lz == kChunkDepth-1)  rebuildChunk(cx, cz + 1);
  }
}

void sortNearestFirst(std::vector<Chunk *> &chunks, const glm::vec3 &viewer) {
  std::vector<std::pair<float, Chunk *>> by_distance;
  by_distance.reserve(chunks.size());
  for (Chunk *c : chunks) {
    glm::vec2 centre = (glm::vec2(c->getPos()) + 0.5f) * glm::vec2(kChunkWidth, kChunkDepth);
    glm::vec2 d = centre - glm::vec2(viewer.x, viewer.z);
    by_distance.emplace_back(glm::dot(d, d), c);
  }
  std::sort(by_distance.begin(), by_distance.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  for (size_t i = 0; i < chunks.size(); ++i) chunks[i] = by_distance[i].second;
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
                   int viewportWidth, int viewportHeight,
                   const glm::vec3 &viewer_pos, bool viewer_underwater,
                   const std::function<void()> &under_water) {
  glm::mat4 viewProj = projection * view;
  glm::vec4 planes[6];
  Camera::getFrustumPlanes(planes, viewProj);

  // Chunks can come and go while the lock is let go for under_water, so the
  // visible list is gathered afresh under each hold.
  auto visible = [&] {
    std::vector<Chunk *> out;
    out.reserve(chunks.size());
    for (const auto &[key, chunk] : chunks) {
      if (!chunk) continue;
      glm::vec3 min = {key.x * kChunkWidth, 0.0f, key.z * kChunkDepth};
      glm::vec3 max = {(key.x + 1) * kChunkWidth,
                       static_cast<float>(kChunkHeight),
                       (key.z + 1) * kChunkDepth};
      if (isChunkInFrustum(planes, min, max)) out.push_back(chunk.get());
    }
    return out;
  };

  {
    ReadLock lock(chunks_mutex);
    // Shadow pass: render depth from sun's perspective (uses all loaded chunks
    // so that off-screen geometry can still cast shadows into view)
    renderer->shadowPass(chunks, viewer_pos, timeOfDay,
                         viewportWidth, viewportHeight);

    // Sky must be drawn before world geometry (it uses GL_LEQUAL depth and writes
    // no depth values, so any chunk fragment will correctly overwrite it).
    // Underwater there's no sky to see: the background is the water fog's
    // colour, so the terrain fading into that fog never meets a bright edge.
    if (viewer_underwater) {
      glm::vec3 murk = Renderer::sceneLighting(timeOfDay).water_fog_color;
      glClearColor(murk.r, murk.g, murk.b, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
    } else {
      renderer->drawSky(view, projection, timeOfDay);
    }

    // Nearest first: the GPU skips shading what's already hidden.
    std::vector<Chunk *> opaque = visible();
    sortNearestFirst(opaque, viewer_pos);
    renderer->drawOpaqueChunks(opaque, view, projection,
                               viewer_pos, viewer_underwater, timeOfDay);
  }

  if (under_water) under_water();

  ReadLock lock(chunks_mutex);
  renderer->drawTransparentChunks(visible(), view, projection,
                                  viewer_pos, viewer_underwater, timeOfDay);

  // Clouds drawn after opaque & transparent terrain so they blend correctly
  // with the sky behind them while terrain in front occludes them via depth.
  if (!viewer_underwater)
    renderer->drawClouds(view, projection, viewer_pos, timeOfDay, cloud_time);
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
  return blocksMovement(getBlockAt(glm::vec3(bx + 0.5f, by + 0.5f, bz + 0.5f)));
}

CellBoxes World::boxesAt(glm::ivec3 p) const {
  if (p.y < 0 || p.y >= kChunkHeight) return {};
  const BlockType b = getBlockAt(glm::vec3(p) + 0.5f);
  if (!isFence(b)) return blockBoxes(b);
  BlockType around[4];
  for (int i = 0; i < 4; ++i) around[i] = getBlockAt(glm::vec3(p + kSides[i]) + 0.5f);
  return fenceBoxes(b, around);
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
                              float max_distance, bool liquids) const {
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

    if (block != BlockType::AIR && (liquids || !isLiquid(block))) {
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


WorldEdits World::snapshotEdits() const {
  std::lock_guard lk(edits_mutex);
  return edits;
}

// ─── Remote edits ──────────────────────────────────────────────────────

void World::setLoadedEdits(std::span<const BlockEdit> list) {
  WorldEdits e;
  for (const BlockEdit &edit : list) {
    if (edit.pos.y < 0 || edit.pos.y >= kChunkHeight) continue;
    int cx = static_cast<int>(std::floor(static_cast<float>(edit.pos.x) / kChunkWidth));
    int cz = static_cast<int>(std::floor(static_cast<float>(edit.pos.z) / kChunkDepth));
    int lx = edit.pos.x - cx * kChunkWidth;
    int lz = edit.pos.z - cz * kChunkDepth;
    e[{cx, cz}][lx + kChunkWidth * (lz + kChunkDepth * edit.pos.y)] =
        static_cast<uint8_t>(edit.type);
  }
  setLoadedEdits(std::move(e));
}

std::optional<BlockType> World::editAt(const glm::ivec3 &pos) const {
  if (pos.y < 0 || pos.y >= kChunkHeight) return std::nullopt;
  int cx = static_cast<int>(std::floor(static_cast<float>(pos.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(pos.z) / kChunkDepth));
  uint32_t idx = (pos.x - cx * kChunkWidth) + kChunkWidth * ((pos.z - cz * kChunkDepth) +
                                                             kChunkDepth * pos.y);
  std::lock_guard lk(edits_mutex);
  auto chunk = edits.find({cx, cz});
  if (chunk == edits.end()) return std::nullopt;
  auto it = chunk->second.find(idx);
  if (it == chunk->second.end()) return std::nullopt;
  return static_cast<BlockType>(it->second);
}

std::vector<BlockEdit> World::editList() const {
  std::vector<BlockEdit> out;
  std::lock_guard lk(edits_mutex);
  for (const auto &[key, chunk_edits] : edits)
    for (const auto &[idx, block] : chunk_edits) {
      int lx = static_cast<int>(idx % kChunkWidth);
      int lz = static_cast<int>((idx / kChunkWidth) % kChunkDepth);
      int y = static_cast<int>(idx / (kChunkWidth * kChunkDepth));
      out.push_back({{key.x * kChunkWidth + lx, y, key.z * kChunkDepth + lz},
                     static_cast<BlockType>(block)});
    }
  return out;
}

void World::applyEdit(const glm::ivec3 &pos, BlockType type) {
  if (pos.y < 0 || pos.y >= kChunkHeight) return;
  int cx = static_cast<int>(std::floor(static_cast<float>(pos.x) / kChunkWidth));
  int cz = static_cast<int>(std::floor(static_cast<float>(pos.z) / kChunkDepth));
  if (getChunk(cx, cz)) {
    setBlockAt(pos, type);
    return;
  }
  // Record it for chunks generated from now on, and remember it in case the
  // chunk is being generated right now.
  int lx = pos.x - cx * kChunkWidth;
  int lz = pos.z - cz * kChunkDepth;
  uint32_t idx = lx + kChunkWidth * (lz + kChunkDepth * pos.y);
  {
    std::lock_guard lk(edits_mutex);
    edits[{cx, cz}][idx] = static_cast<uint8_t>(type);
  }
  pending_edits.push_back({pos, type});
}

void World::flushPendingEdits() {
  std::erase_if(pending_edits, [&](const BlockEdit &e) {
    int cx = static_cast<int>(std::floor(static_cast<float>(e.pos.x) / kChunkWidth));
    int cz = static_cast<int>(std::floor(static_cast<float>(e.pos.z) / kChunkDepth));
    if (!getChunk(cx, cz)) return false;
    if (getBlockAt(glm::vec3(e.pos) + 0.5f) != e.type) setBlockAt(e.pos, e.type);
    return true;
  });
}
