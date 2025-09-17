#include "world.hpp"
#include "greedy_mesher.hpp"

World::World() {
  // Create a single chunk at (0, 0)
  // loadChunk(0, 0);
  const int R = 1; // radius in chunks (1 => 3x3)
  for (int dx = -R; dx <= R; ++dx)
    for (int dz = -R; dz <= R; ++dz)
      loadChunk(dx, dz);
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

static inline int floorDiv(int a, int b) {
  int q = a / b, r = a % b;
  if (r != 0 && ((r < 0) != (b < 0)))
    --q;
  return q;
}
static inline int floorMod(int a, int b) {
  int r = a % b;
  return (r < 0) ? r + b : r;
}

BlockType World::getBlock(int x, int y, int z) const {
  if (y < 0 || y >= Chunk::H)
    return BlockType::AIR;

  const int cx = floorDiv(x, Chunk::W);
  const int cz = floorDiv(z, Chunk::L);
  const uint64_t key = makeChunkKey(cx, cz);
  auto it = chunks.find(key);
  if (it == chunks.end())
    return BlockType::AIR;

  const int lx = floorMod(x, Chunk::W);
  const int lz = floorMod(z, Chunk::L);
  return it->second->getBlock(lx, y, lz);
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
  if (dirty.empty())
    return;

  auto sample = [&](int gx, int gy, int gz) -> BlockType {
    return getBlock(gx, gy, gz);
  };

  for (auto key : dirty) {
    auto it = chunks.find(key);
    if (it == chunks.end())
      continue;
    Chunk &c = *it->second;
    GreedyMesher::build(c, sample, c.opaqueMesh, c.transparentMesh);
  }
  dirty.clear();
}

void World::loadChunk(int cx, int cz) {
  const uint64_t key = makeChunkKey(cx, cz);
  if (chunks.find(key) != chunks.end())
    return;

  auto &up = chunks[key] = std::make_unique<Chunk>(cx, cz);
  up->generateChunk();

  // mark this chunk dirty
  dirty.insert(key);

  // if neighbors exist, mark them dirty too so shared faces get culled
  auto mark_if_present = [&](int nx, int nz) {
    auto it = chunks.find(makeChunkKey(nx, nz));
    if (it != chunks.end())
      dirty.insert(makeChunkKey(nx, nz));
  };
  mark_if_present(cx + 1, cz);
  mark_if_present(cx - 1, cz);
  mark_if_present(cx, cz + 1);
  mark_if_present(cx, cz - 1);
}

void World::unloadChunk(int cx, int cz) {
  uint64_t key = makeChunkKey(cx, cz);
  chunks.erase(key);
}
