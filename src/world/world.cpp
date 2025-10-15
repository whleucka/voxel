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

  spiral_offsets = generateSpiralOrder(kRenderDistance);

  glm::vec3 pos = {kChunkWidth * 0.5f, 125.0f, kChunkDepth * 0.5f};

  player->setPosition(pos);
  player->getCamera().setPosition(pos);

  // Now compute chunk center and kick off loading
  const glm::vec3 p = player->getPosition();
  last_chunk_x = static_cast<int>(std::floor(p.x / kChunkWidth));
  last_chunk_z = static_cast<int>(std::floor(p.z / kChunkDepth));
  updateLoadedChunks();
}

std::vector<glm::ivec2> World::generateSpiralOrder(int radius) {
  std::vector<glm::ivec2> spiral;
  spiral.reserve((2 * radius + 1) * (2 * radius + 1));

  int x = 0, z = 0;
  int dx = 0, dz = -1;
  int max = (2 * radius + 1);

  for (int i = 0; i < max * max; ++i) {
    if (std::abs(x) <= radius && std::abs(z) <= radius)
      spiral.emplace_back(x, z);

    // turn right at corners
    if (x == z || (x < 0 && x == -z) || (x > 0 && x == 1 - z)) {
      int tmp = dx;
      dx = -dz;
      dz = tmp;
    }

    x += dx;
    z += dz;
  }

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

void World::updateLoadedChunks() {
  constexpr int r = kRenderDistance;
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

void World::render(glm::mat4 &view, glm::mat4 &projection) {
  ReadLock lock(chunks_mutex);
  renderer->drawChunks(chunks, view, projection);
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
