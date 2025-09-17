#include "greedy_mesher.hpp"
#include "block_data_manager.hpp"
#include "block_type.hpp"
#include "texture_manager.hpp"
#include <vector>

static inline bool isAir(BlockType t) { return t == BlockType::AIR; }
static inline bool isTransparent(BlockType t) { return t == BlockType::WATER; }
static inline bool isOpaque(BlockType t) {
  return !isAir(t) && !isTransparent(t);
}

static inline void tileToUV(float tileX, float tileY, float atlasSizePx,
                            float tileSizePx, float &u0, float &v0, float &u1,
                            float &v1) {
  const float TILE = tileSizePx / atlasSizePx;
  u0 = tileX * TILE;
  v0 = 1.0f - (tileY + 1) * TILE; // V-flip because stbi flips the image
  u1 = u0 + TILE;
  v1 = v0 + TILE;
}

static inline void pushQuad(std::vector<Vertex> &verts,
                            std::vector<unsigned int> &indices,
                            const glm::vec3 pos[4], const glm::vec3 &n,
                            float u0, float v0, float u1, float v1) {
  const unsigned int base = static_cast<unsigned int>(verts.size());
  verts.push_back({pos[0], n, {u0, v0}});
  verts.push_back({pos[1], n, {u1, v0}});
  verts.push_back({pos[2], n, {u1, v1}});
  verts.push_back({pos[3], n, {u0, v1}});
  indices.push_back(base + 0);
  indices.push_back(base + 1);
  indices.push_back(base + 2);
  indices.push_back(base + 0);
  indices.push_back(base + 2);
  indices.push_back(base + 3);
}

void GreedyMesher::build(const Chunk &chunk, SampleFn sample, Mesh &opaque,
                         Mesh &transparent) {
  constexpr float ATLAS_SIZE_PX = 256.0f;
  constexpr float TILE_SIZE_PX = 16.0f;

  std::vector<Vertex> opaque_vertices;
  std::vector<unsigned int> opaque_indices;
  std::vector<Texture> opaque_textures;

  std::vector<Vertex> transparent_vertices;
  std::vector<unsigned int> transparent_indices;
  std::vector<Texture> transparent_textures;

  opaque_textures.push_back(
      TextureManager::getInstance().getTexture("block_atlas"));
  transparent_textures.push_back(
      TextureManager::getInstance().getTexture("block_atlas"));

  const int baseX = chunk.world_x * Chunk::W;
  const int baseZ = chunk.world_z * Chunk::L;

  for (int y = 0; y < Chunk::H; ++y) {
    for (int x = 0; x < Chunk::W; ++x) {
      for (int z = 0; z < Chunk::L; ++z) {
        const int gx = baseX + x;
        const int gz = baseZ + z;

        const BlockType bt = sample(gx, y, gz);
        if (isAir(bt))
          continue;

        const bool curIsTransp = isTransparent(bt);
        auto &V = curIsTransp ? transparent_vertices : opaque_vertices;
        auto &I = curIsTransp ? transparent_indices : opaque_indices;

        auto nb = [&](int dx, int dy, int dz) {
          return sample(gx + dx, y + dy, gz + dz);
        };

        auto faceUV = [&](BlockFace face, float &u0, float &v0, float &u1,
                          float &v1) {
          const glm::vec2 tile =
              BlockDataManager::getInstance().getUV(bt, face);
          tileToUV(tile.x, tile.y, ATLAS_SIZE_PX, TILE_SIZE_PX, u0, v0, u1, v1);
        };

        // +Z (FRONT)
        {
          const BlockType nbt = nb(0, 0, +1);
          const bool draw = isAir(nbt) ||
                            (isOpaque(bt) && isTransparent(nbt)) ||
                            (isTransparent(bt) && isOpaque(nbt));
          if (draw) {
            float u0, v0, u1, v1;
            faceUV(BlockFace::FRONT, u0, v0, u1, v1);
            glm::vec3 p[4] = {{x, y, z + 1},
                              {x + 1, y, z + 1},
                              {x + 1, y + 1, z + 1},
                              {x, y + 1, z + 1}};
            pushQuad(V, I, p, {0, 0, 1}, u0, v0, u1, v1);
          }
        }

        // -Z (BACK)
        {
          const BlockType nbt = nb(0, 0, -1);
          const bool draw = isAir(nbt) ||
                            (isOpaque(bt) && isTransparent(nbt)) ||
                            (isTransparent(bt) && isOpaque(nbt));
          if (draw) {
            float u0, v0, u1, v1;
            faceUV(BlockFace::BACK, u0, v0, u1, v1);
            // CCW when viewed from -Z (outside)
            glm::vec3 p[4] = {
                {x + 1, y, z},    // bl
                {x, y, z},        // br
                {x, y + 1, z},    // tr
                {x + 1, y + 1, z} // tl
            };
            pushQuad(V, I, p, {0, 0, -1}, u0, v0, u1, v1);
          }
        }

        // +X (RIGHT)
        {
          const BlockType nbt = nb(+1, 0, 0);
          const bool draw = isAir(nbt) ||
                            (isOpaque(bt) && isTransparent(nbt)) ||
                            (isTransparent(bt) && isOpaque(nbt));
          if (draw) {
            float u0, v0, u1, v1;
            faceUV(BlockFace::RIGHT, u0, v0, u1, v1);
            // CCW when viewed from +X (outside)
            glm::vec3 p[4] = {
                {x + 1, y, z + 1},    // bl
                {x + 1, y, z},        // br
                {x + 1, y + 1, z},    // tr
                {x + 1, y + 1, z + 1} // tl
            };
            pushQuad(V, I, p, {1, 0, 0}, u0, v0, u1, v1);
          }
        }

        // -X (LEFT)  normal = (-1,0,0)
        {
          const BlockType nbt = nb(-1, 0, 0);
          const bool draw = isAir(nbt) ||
                            (isOpaque(bt) && isTransparent(nbt)) ||
                            (isTransparent(bt) && isOpaque(nbt));
          if (draw) {
            float u0, v0, u1, v1;
            faceUV(BlockFace::LEFT, u0, v0, u1, v1);
            // CCW when looking from -X (outside)
            glm::vec3 p[4] = {
                {x, y, z},         // bl
                {x, y, z + 1},     // br
                {x, y + 1, z + 1}, // tr
                {x, y + 1, z}      // tl
            };
            pushQuad(V, I, p, {-1, 0, 0}, u0, v0, u1, v1);
          }
        }

        // +Y (TOP)
        {
          const BlockType nbt = nb(0, +1, 0);
          const bool draw = isAir(nbt) ||
                            (isOpaque(bt) && isTransparent(nbt)) ||
                            (isTransparent(bt) && isOpaque(nbt));
          if (draw) {
            float u0, v0, u1, v1;
            faceUV(BlockFace::TOP, u0, v0, u1, v1);
            glm::vec3 p[4] = {{x, y + 1, z + 1},
                              {x + 1, y + 1, z + 1},
                              {x + 1, y + 1, z},
                              {x, y + 1, z}};
            pushQuad(V, I, p, {0, 1, 0}, u0, v0, u1, v1);
          }
        }

        // -Y (BOTTOM)
        {
          const BlockType nbt = nb(0, -1, 0);
          const bool draw = isAir(nbt) ||
                            (isOpaque(bt) && isTransparent(nbt)) ||
                            (isTransparent(bt) && isOpaque(nbt));
          if (draw) {
            float u0, v0, u1, v1;
            faceUV(BlockFace::BOTTOM, u0, v0, u1, v1);
            glm::vec3 p[4] = {
                {x, y, z}, {x + 1, y, z}, {x + 1, y, z + 1}, {x, y, z + 1}};
            pushQuad(V, I, p, {0, -1, 0}, u0, v0, u1, v1);
          }
        }
      }
    }
  }

  // opaque
  if (opaque_indices.empty()) {
    opaque.clear(); // optional
  } else {
    opaque.vertices = std::move(opaque_vertices);
    opaque.indices = std::move(opaque_indices);
    opaque.setupMesh();
  }

  // transparent
  if (transparent_indices.empty()) {
    transparent.clear(); // optional
  } else {
    transparent.vertices = std::move(transparent_vertices);
    transparent.indices = std::move(transparent_indices);
    transparent.setupMesh();
  }
}
