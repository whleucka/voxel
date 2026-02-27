#include "world/world.hpp"
#include "block/block_data.hpp"
#include "core/constants.hpp"
#include "render/renderer.hpp"
#include "util/lock.hpp"
#include <cmath>
#include <glm/fwd.hpp>
#include <memory>

World::World()
    : renderer(std::make_unique<Renderer>()),
      player(std::make_unique<Player>(glm::vec3(0.0f, 0.0f, 0.0f))) {}

void World::init() {
  renderer->init();

  spiral_offsets = generateSpiralOrder(kRenderDistance / kChunkWidth);

  glm::vec3 pos = {kChunkWidth * 0.5f, 125.0f, kChunkDepth * 0.5f};

  player->setPosition(pos);
  player->getCamera().setPosition(pos);

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
  });
}

void World::update(float dt) {
  player->update(dt, this);

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
  constexpr int r = kRenderDistance / kChunkWidth;
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

void World::render(glm::mat4 &view, glm::mat4 &projection, float timeOfDay) {
  // Sky must be drawn before world geometry (it uses GL_LEQUAL depth and writes
  // no depth values, so any chunk fragment will correctly overwrite it).
  renderer->drawSky(view, projection, timeOfDay);

  ReadLock lock(chunks_mutex);

  glm::mat4 viewProj = projection * view;

  glm::vec4 planes[6];
  player->getCamera().getFrustumPlanes(planes, viewProj);

  robin_hood::unordered_map<ChunkKey, std::shared_ptr<Chunk>, ChunkKeyHash>
      visibleChunks;
  visibleChunks.reserve(chunks.size());

  for (const auto &[key, chunk] : chunks) {
    glm::vec3 min = {key.x * kChunkWidth, 0.0f, key.z * kChunkDepth};
    glm::vec3 max = {(key.x + 1) * kChunkWidth,
                     static_cast<float>(kChunkHeight),
                     (key.z + 1) * kChunkDepth};

    if (isChunkInFrustum(planes, min, max)) {
      visibleChunks.emplace(key, chunk);
    }
  }

  renderer->drawChunks(visibleChunks, view, projection,
                       player->getPosition(), player->isUnderwater(), timeOfDay);
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

// ─── DDA Raycast ───────────────────────────────────────────────────────
// Steps through the voxel grid one cell at a time along a ray.
// Based on "A Fast Voxel Traversal Algorithm" (Amanatides & Woo, 1987).

RaycastResult World::raycast(const glm::vec3& origin, const glm::vec3& direction,
                              float max_distance) const {
  RaycastResult result;

  if (glm::length(direction) < 1e-8f) return result;

  glm::vec3 dir = glm::normalize(direction);

  // Current voxel position
  int x = static_cast<int>(std::floor(origin.x));
  int y = static_cast<int>(std::floor(origin.y));
  int z = static_cast<int>(std::floor(origin.z));

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
    tMaxX = ((stepX > 0) ? (std::floor(origin.x) + 1.0f - origin.x)
                          : (origin.x - std::floor(origin.x))) * tDeltaX;
  } else {
    tDeltaX = 1e30f;
    tMaxX = 1e30f;
  }

  if (std::abs(dir.y) > 1e-8f) {
    tDeltaY = std::abs(1.0f / dir.y);
    tMaxY = ((stepY > 0) ? (std::floor(origin.y) + 1.0f - origin.y)
                          : (origin.y - std::floor(origin.y))) * tDeltaY;
  } else {
    tDeltaY = 1e30f;
    tMaxY = 1e30f;
  }

  if (std::abs(dir.z) > 1e-8f) {
    tDeltaZ = std::abs(1.0f / dir.z);
    tMaxZ = ((stepZ > 0) ? (std::floor(origin.z) + 1.0f - origin.z)
                          : (origin.z - std::floor(origin.z))) * tDeltaZ;
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
