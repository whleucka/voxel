#include "greedy_mesher.hpp"
#include "block_data_manager.hpp"
#include "block_type.hpp"
#include "texture_manager.hpp"
#include <functional>
#include <vector>

// ----- helpers -----

static inline bool isAir(BlockType t) { return t == BlockType::AIR; }
static inline bool isTrans(BlockType t) { return t == BlockType::WATER; }
static inline bool isOpaque(BlockType t) { return !isAir(t) && !isTrans(t); }

static inline void tileToUV(float tileX, float tileY, float atlasSizePx,
                            float tileSizePx, float &u0, float &v0, float &u1,
                            float &v1) {
  const float TILE = tileSizePx / atlasSizePx;
  const float pad = 0.5f / atlasSizePx; // shrink in by half a texel
  u0 = tileX * TILE + pad;
  v0 = 1.0f - (tileY + 1) * TILE + pad; // flipped V
  u1 = (tileX + 1) * TILE - pad;
  v1 = 1.0f - tileY * TILE - pad;
}

struct MaskCell {
  bool set = false;
  glm::vec2 tile = {0, 0}; // atlas tile (integer coords in json)
};

// rectangle merge on a UxV mask, emitting one quad per rect
template <typename EmitFn, typename EQ>
static void greedy2D(int U, int V, std::vector<MaskCell> &mask, EmitFn emit,
                     EQ equal) {
  for (int v = 0; v < V; ++v) {
    for (int u = 0; u < U;) {
      int idx = u + v * U;
      if (!mask[idx].set) {
        ++u;
        continue;
      }

      // width
      int w = 1;
      while (u + w < U && equal(mask[idx], mask[idx + w]))
        ++w;

      // height
      int h = 1;
      bool stop = false;
      while (v + h < V && !stop) {
        for (int k = 0; k < w; ++k) {
          if (!equal(mask[idx], mask[u + k + (v + h) * U])) {
            stop = true;
            break;
          }
        }
        if (!stop)
          ++h;
      }

      // emit rect
      emit(u, v, w, h, mask[idx]);

      // clear area
      for (int dv = 0; dv < h; ++dv)
        for (int du = 0; du < w; ++du)
          mask[(u + du) + (v + dv) * U].set = false;

      u += w;
    }
  }
}

static inline void pushQuad(std::vector<Vertex> &verts,
                            std::vector<unsigned int> &inds,
                            const glm::vec3 p[4], const glm::vec3 &n, float u0,
                            float v0, float u1, float v1) {
  const unsigned base = (unsigned)verts.size();
  verts.push_back({p[0], n, {u0, v0}});
  verts.push_back({p[1], n, {u1, v0}});
  verts.push_back({p[2], n, {u1, v1}});
  verts.push_back({p[3], n, {u0, v1}});
  inds.insert(inds.end(),
              {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});
}

// ----- build (greedy) -----

void GreedyMesher::build(const Chunk &chunk, SampleFn sample, Mesh &opaque,
                         Mesh &transparent) {
  constexpr float ATLAS = 256.0f, TILE = 16.0f;

  std::vector<Vertex> vO, vT;
  std::vector<unsigned> iO, iT;

  auto same = [](const MaskCell &a, const MaskCell &b) {
    return a.set && b.set && a.tile == b.tile;
  };

  const int X = Chunk::W, Y = Chunk::H, Z = Chunk::L;
  const int baseX = chunk.world_x * Chunk::W;
  const int baseZ = chunk.world_z * Chunk::L;

  // -------- ±Y (TOP/BOTTOM) --------
  {
    std::vector<MaskCell> mask(X * Z);

    // TOP (+Y)
    for (int y = 0; y < Y; ++y) {
      // fill mask: visible if current is non-air and neighbor(y+1) differs in
      // opacity or is air
      for (int z = 0; z < Z; ++z) {
        for (int x = 0; x < X; ++x) {
          const BlockType bt = sample(baseX + x, y, baseZ + z);
          if (isAir(bt)) {
            mask[x + z * X].set = false;
            continue;
          }
          const BlockType nb = (y + 1 < Y) ? sample(baseX + x, y + 1, baseZ + z)
                                           : BlockType::AIR;
          const bool draw = isAir(nb) || (isOpaque(bt) != isOpaque(nb));
          if (!draw) {
            mask[x + z * X].set = false;
            continue;
          }
          mask[x + z * X].set = true;
          mask[x + z * X].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::TOP);
        }
      }
      auto emit = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0, u1, v1;
        tileToUV(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0, u1, v1);
        glm::vec3 p[4] = {
            {(float)u, (float)(y + 1), (float)(v + h)},
            {(float)(u + w), (float)(y + 1), (float)(v + h)},
            {(float)(u + w), (float)(y + 1), (float)v},
            {(float)u, (float)(y + 1), (float)v},
        };
        // group by opacity of "current" voxel
        const BlockType any = sample(baseX + u, y, baseZ + v);
        auto &VV = isTrans(any) ? vT : vO;
        auto &II = isTrans(any) ? iT : iO;
        pushQuad(VV, II, p, {0, 1, 0}, u0, v0, u1, v1);
      };
      greedy2D(X, Z, mask, emit, same);
    }

    // BOTTOM (−Y)
    for (int y = 0; y < Y; ++y) {
      for (int z = 0; z < Z; ++z) {
        for (int x = 0; x < X; ++x) {
          const BlockType bt = sample(baseX + x, y, baseZ + z);
          if (isAir(bt)) {
            mask[x + z * X].set = false;
            continue;
          }
          const BlockType nb = (y - 1 >= 0)
                                   ? sample(baseX + x, y - 1, baseZ + z)
                                   : BlockType::AIR;
          const bool draw = isAir(nb) || (isOpaque(bt) != isOpaque(nb));
          if (!draw) {
            mask[x + z * X].set = false;
            continue;
          }
          mask[x + z * X].set = true;
          mask[x + z * X].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::BOTTOM);
        }
      }
      auto emit = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0, u1, v1;
        tileToUV(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0, u1, v1);
        glm::vec3 p[4] = {
            {(float)u, (float)y, (float)v},
            {(float)(u + w), (float)y, (float)v},
            {(float)(u + w), (float)y, (float)(v + h)},
            {(float)u, (float)y, (float)(v + h)},
        };
        const BlockType any = sample(baseX + u, y, baseZ + v);
        auto &VV = isTrans(any) ? vT : vO;
        auto &II = isTrans(any) ? iT : iO;
        pushQuad(VV, II, p, {0, -1, 0}, u0, v0, u1, v1);
      };
      greedy2D(X, Z, mask, emit, same);
    }
  }

  // -------- ±Z (FRONT/BACK) --------
  {
    std::vector<MaskCell> mask(X * Y);

    // FRONT (+Z)
    for (int z = 0; z < Z; ++z) {
      for (int y = 0; y < Y; ++y) {
        for (int x = 0; x < X; ++x) {
          const BlockType bt = sample(baseX + x, y, baseZ + z);
          if (isAir(bt)) {
            mask[x + y * X].set = false;
            continue;
          }
          const BlockType nb = sample(baseX + x, y, baseZ + z + 1);
          const bool draw = isAir(nb) || (isOpaque(bt) != isOpaque(nb));
          if (!draw) {
            mask[x + y * X].set = false;
            continue;
          }
          mask[x + y * X].set = true;
          mask[x + y * X].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::FRONT);
        }
      }
      auto emit = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0, u1, v1;
        tileToUV(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0, u1, v1);
        glm::vec3 p[4] = {
            {(float)u, (float)v, (float)(z + 1)},
            {(float)(u + w), (float)v, (float)(z + 1)},
            {(float)(u + w), (float)(v + h), (float)(z + 1)},
            {(float)u, (float)(v + h), (float)(z + 1)},
        };
        const BlockType any = sample(baseX + u, v, baseZ + z);
        auto &VV = isTrans(any) ? vT : vO;
        auto &II = isTrans(any) ? iT : iO;
        pushQuad(VV, II, p, {0, 0, 1}, u0, v0, u1, v1);
      };
      greedy2D(X, Y, mask, emit, same);
    }

    // BACK (−Z)
    for (int z = 0; z < Z; ++z) {
      for (int y = 0; y < Y; ++y) {
        for (int x = 0; x < X; ++x) {
          const BlockType bt = sample(baseX + x, y, baseZ + z);
          if (isAir(bt)) {
            mask[x + y * X].set = false;
            continue;
          }
          const BlockType nb = sample(baseX + x, y, baseZ + z - 1);
          const bool draw = isAir(nb) || (isOpaque(bt) != isOpaque(nb));
          if (!draw) {
            mask[x + y * X].set = false;
            continue;
          }
          mask[x + y * X].set = true;
          mask[x + y * X].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::BACK);
        }
      }
      auto emit = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0, u1, v1;
        tileToUV(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0, u1, v1);
        glm::vec3 p[4] = {
            {(float)(u + w), (float)v, (float)z},
            {(float)u, (float)v, (float)z},
            {(float)u, (float)(v + h), (float)z},
            {(float)(u + w), (float)(v + h), (float)z},
        };
        const BlockType any = sample(baseX + u, v, baseZ + z);
        auto &VV = isTrans(any) ? vT : vO;
        auto &II = isTrans(any) ? iT : iO;
        pushQuad(VV, II, p, {0, 0, -1}, u0, v0, u1, v1);
      };
      greedy2D(X, Y, mask, emit, same);
    }
  }

  // -------- ±X (RIGHT/LEFT) --------
  {
    std::vector<MaskCell> mask(Z * Y); // U=Z, V=Y

    // RIGHT (+X)
    for (int x = 0; x < X; ++x) {
      for (int y = 0; y < Y; ++y) {
        for (int z = 0; z < Z; ++z) {
          const BlockType bt = sample(baseX + x, y, baseZ + z);
          if (isAir(bt)) {
            mask[z + y * Z].set = false;
            continue;
          }
          const BlockType nb = sample(baseX + x + 1, y, baseZ + z);
          const bool draw = isAir(nb) || (isOpaque(bt) != isOpaque(nb));
          if (!draw) {
            mask[z + y * Z].set = false;
            continue;
          }
          mask[z + y * Z].set = true;
          mask[z + y * Z].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::RIGHT);
        }
      }
      auto emit = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0, u1, v1;
        tileToUV(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0, u1, v1);
        glm::vec3 p[4] = {
            {(float)(x + 1), (float)v, (float)(u + w)},      // bl
            {(float)(x + 1), (float)v, (float)u},            // br
            {(float)(x + 1), (float)(v + h), (float)u},      // tr
            {(float)(x + 1), (float)(v + h), (float)(u + w)} // tl
        };
        const BlockType any = sample(baseX + x, v, baseZ + u);
        auto &VV = isTrans(any) ? vT : vO;
        auto &II = isTrans(any) ? iT : iO;
        pushQuad(VV, II, p, {1, 0, 0}, u0, v0, u1, v1);
      };
      greedy2D(Z, Y, mask, emit, same);
    }

    // LEFT (−X)
    for (int x = 0; x < X; ++x) {
      for (int y = 0; y < Y; ++y) {
        for (int z = 0; z < Z; ++z) {
          const BlockType bt = sample(baseX + x, y, baseZ + z);
          if (isAir(bt)) {
            mask[z + y * Z].set = false;
            continue;
          }
          const BlockType nb = sample(baseX + x - 1, y, baseZ + z);
          const bool draw = isAir(nb) || (isOpaque(bt) != isOpaque(nb));
          if (!draw) {
            mask[z + y * Z].set = false;
            continue;
          }
          mask[z + y * Z].set = true;
          mask[z + y * Z].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::LEFT);
        }
      }
      // LEFT (−X) — plane at x, normal -X, CCW when viewed from outside (−X)
      auto emit = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float U0, V0, U1, V1;
        tileToUV(cell.tile.x, cell.tile.y, ATLAS, TILE, U0, V0, U1, V1);
        glm::vec3 p[4] = {
            {(float)x, (float)v, (float)u},             // bl = (x, y,   z)
            {(float)x, (float)v, (float)(u + w)},       // br = (x, y,   z+w)
            {(float)x, (float)(v + h), (float)(u + w)}, // tr = (x, y+h, z+w)
            {(float)x, (float)(v + h), (float)u}        // tl = (x, y+h, z)
        };
        const BlockType any = sample(baseX + x, v, baseZ + u);
        auto &VV = isTrans(any) ? vT : vO;
        auto &II = isTrans(any) ? iT : iO;
        pushQuad(VV, II, p, {-1, 0, 0}, U0, V0, U1, V1);
      };
      greedy2D(Z, Y, mask, emit, same);
    }
  }

  // upload
  if (!iO.empty()) {
    opaque.vertices = std::move(vO);
    opaque.indices = std::move(iO);
    opaque.setupMesh();
  } else {
    opaque.clear();
  }

  if (!iT.empty()) {
    transparent.vertices = std::move(vT);
    transparent.indices = std::move(iT);
    transparent.setupMesh();
  } else {
    transparent.clear();
  }
}
