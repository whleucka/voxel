#pragma once

#include "block/block_type.hpp"
#include "chunk/chunk_mesh.hpp"
#include "core/constants.hpp"
#include <array>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <mutex>
#include <queue>
#include <tuple>
#include <vector>

class World;

struct ChunkKey {
  int x, z;
  bool operator==(const ChunkKey &other) const noexcept {
    return x == other.x && z == other.z;
  }
};

struct ChunkKeyHash {
  std::size_t operator()(const ChunkKey &k) const noexcept {
    return (std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) << 1));
  }
};

class Chunk {
public:
  Chunk(int x, int z);
  ~Chunk() = default;

  void init();
  void buildMeshData(World* world, TextureManager& texture_manager);
  void uploadGPU();

  BlockType &at(int x, int y, int z);
  const BlockType &at(int x, int y, int z) const;
  BlockType safeAt(int x, int y, int z) const;

  // Overwrite a block by its linear index (used when replaying saved edits).
  void setBlockLinear(uint32_t index, BlockType type);

  void computeSkyLight();
  uint8_t getSkyLight(int x, int y, int z) const;
  uint8_t safeSkyLight(int x, int y, int z) const;

  void computeBlockLight();
  uint8_t getBlockLight(int x, int y, int z) const;
  uint8_t safeBlockLight(int x, int y, int z) const;

  // Block light across a chunk border. Faces: 0 = -x, 1 = +x, 2 = -z, 3 = +z;
  // the neighbour across face f faces back with f ^ 1.
  static_assert(kChunkWidth == kChunkDepth, "faces are indexed along one length");
  using BorderLight = std::array<uint8_t, kChunkWidth * kChunkHeight>;
  static constexpr glm::ivec2 kFaceStep[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  // The light in this chunk's layer of blocks on `face`.
  BorderLight borderLight(int face) const;
  // Lets in the light a neighbour shines across `face` and floods it on
  // through this chunk. Light only rises. Returns true if anything changed.
  bool acceptBorderLight(int face, const BorderLight &incoming);

  // Emitter bookkeeping (for the cross-chunk relight fast-path gate).
  bool hasEmitters() const { return emitter_count > 0; }
  int  emitterCount() const { return emitter_count; }
  void adjustEmitterCount(int delta) { emitter_count += delta; }

  // Directly overwrite a block-light value by linear index (used when the
  // World writes cross-chunk light results back into this chunk).
  void setBlockLightLinear(uint32_t index, uint8_t value);

  ChunkMesh &getMesh() { return mesh; }
  glm::mat4 getModelMatrix() const;
  glm::ivec2 getPos() const { return pos; }

  std::mutex data_mutex; // guards blocks[] during concurrent write/mesh-gen

private:
  glm::ivec2 pos;
  std::vector<BlockType> blocks;
  std::vector<uint8_t>   skylight;
  std::vector<uint8_t>   blocklight;
  int emitter_count = 0; // number of light-emitting blocks in this chunk
  ChunkMesh mesh;

  using LightQueue = std::queue<std::tuple<int, int, int>>;
  // Spreads block light outward from every queued block, within this chunk.
  void floodBlockLight(LightQueue &queue);
  // The (x, z) of the i-th block along `face`.
  static glm::ivec2 faceCell(int face, int i);
};
