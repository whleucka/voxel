#include "world.hpp"

World::World() {
  // Create a single chunk at (0, 0)
  loadChunk(0, 0);
}

World::~World() = default;

void World::update(const glm::vec3 &) {
  // For now, we only have one chunk, so there's nothing to do here.
}

std::vector<Chunk *> World::getVisibleChunks(const Camera &) {
  std::vector<Chunk *> result;
  for (auto &[key, chunkPtr] : chunks) {
    result.push_back(chunkPtr.get());
  }
  return result;
}

BlockType World::getBlock(int x, int y, int z) const {
  int cx = x / Chunk::W;
  int cz = z / Chunk::L;
  uint64_t key = makeChunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    return it->second->getBlock(x % Chunk::W, y, z % Chunk::L);
  }
  return BlockType::AIR;
}

void World::setBlock(int x, int y, int z, BlockType type) {
  int cx = x / Chunk::W;
  int cz = z / Chunk::L;
  uint64_t key = makeChunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it != chunks.end()) {
    it->second->setBlock(x % Chunk::W, y, z % Chunk::L, type);
  }
}

void World::generateMeshes() {
  for (auto const& [key, val] : chunks)
  {
    val->generateMesh();
  }
}

void World::loadChunk(int cx, int cz) {
  uint64_t key = makeChunkKey(cx, cz);
  if (chunks.find(key) == chunks.end()) {
    chunks[key] = std::make_unique<Chunk>(cx, cz);
    chunks[key]->generateChunk();
  }
}


void World::unloadChunk(int cx, int cz) {
  uint64_t key = makeChunkKey(cx, cz);
  chunks.erase(key);
}
