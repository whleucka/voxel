#include "world/world.hpp"
#include "core/constants.hpp"
#include "render/renderer.hpp"
#include "util/lock.hpp"
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
  player->update(dt);

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

void World::render(glm::mat4 &view, glm::mat4 &projection) {
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

  renderer->drawChunks(visibleChunks, view, projection);
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
