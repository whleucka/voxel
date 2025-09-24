#include "greedy_mesher.hpp"
#include "block_data_manager.hpp"
#include "block_type.hpp"
#include <functional>
#include <vector>

/** Warning to future self: if you touch something here, expect breakage lol */

// ----- helpers -----
static inline bool isAir(BlockType t) { return t == BlockType::AIR; }

static inline void pushQuadTiled(std::vector<Vertex> &verts,
                                 std::vector<unsigned> &inds,
                                 const glm::vec3 P[4], const glm::vec3 &N,
                                 const glm::vec2 LUV[4],   // in blocks
                                 const glm::vec2 &tileBase // atlas u0,v0
) {
  unsigned base = (unsigned)verts.size();
  for (int i = 0; i < 4; ++i)
    verts.push_back({P[i], N, LUV[i], tileBase});
  inds.insert(inds.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}
static inline void tileBaseNoPad(float tileX, float tileY, float atlasPx,
                                 float tilePx, float &u0, float &v0) {
  const float T = tilePx / atlasPx; // 16 / 256 = 0.0625
  u0 = tileX * T;
  v0 = 1.0f - (tileY + 1) * T; // stbi vertical flip
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

// ----- build (greedy) -----

std::pair<CpuMesh, CpuMesh> GreedyMesher::build_cpu(const Chunk &chunk, SampleFn sample) {
  constexpr float ATLAS = 256.0f, TILE = 16.0f;

  std::vector<Vertex> vO, vT;
  std::vector<unsigned int> iO, iT;

  auto same = [](const MaskCell &a, const MaskCell &b) {
    return a.set && b.set && a.tile == b.tile;
  };

  const int X = Chunk::W, Y = Chunk::H, Z = Chunk::L;
  const int baseX = chunk.world_x * Chunk::W;
  const int baseZ = chunk.world_z * Chunk::L;

  auto edgeFluidNeighbor = [&](BlockType bt, int x, int y, int z, int nx, int nz) {
    BlockType nb = sample(baseX + x + nx, y, baseZ + z + nz);

    // Only apply to fluids (water) on chunk borders for SIDE faces.
    if (!BlockDataManager::getInstance().isFluid(bt)) return nb;

    bool atXPosEdge = (nx > 0) && (x == X - 1);
    bool atXNegEdge = (nx < 0) && (x == 0);
    bool atZPosEdge = (nz > 0) && (z == Z - 1);
    bool atZNegEdge = (nz < 0) && (z == 0);

    if (atXPosEdge || atXNegEdge || atZPosEdge || atZNegEdge) {
      // Pretend neighbor is the same fluid → no side face at chunk boundary.
      nb = bt;
    }
    return nb;
  };

  std::vector<MaskCell> mask;
  mask.reserve(std::max({Chunk::W * Chunk::H, Chunk::W * Chunk::L, Chunk::H * Chunk::L}));

  // -------- ±Y (TOP/BOTTOM) --------
  {
    mask.assign(X * Z, MaskCell());

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
          const bool draw = isAir(nb) || (BlockDataManager::getInstance().isOpaque(bt) != BlockDataManager::getInstance().isOpaque(nb));
          if (!draw) {
            mask[x + z * X].set = false;
            continue;
          }
          mask[x + z * X].set = true;
          mask[x + z * X].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::TOP);
        }
      }
      auto emitTop = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0;
        tileBaseNoPad(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0);
        glm::vec2 tileBase(u0, v0);

        glm::vec3 P[4] = {
            {(float)u, (float)(y + 1), (float)(v + h)},
            {(float)(u + w), (float)(y + 1), (float)(v + h)},
            {(float)(u + w), (float)(y + 1), (float)v},
            {(float)u, (float)(y + 1), (float)v},
        };
        // match P[] CCW; (X,Z) -> (w,h) in blocks
        glm::vec2 LUV[4] = {{0, h}, {w, h}, {w, 0}, {0, 0}};
        auto &VV = BlockDataManager::getInstance().isTransparent(sample(baseX + u, y, baseZ + v)) ? vT : vO;
        auto &II = BlockDataManager::getInstance().isTransparent(sample(baseX + u, y, baseZ + v)) ? iT : iO;
        pushQuadTiled(VV, II, P, {0, 1, 0}, LUV, tileBase);
      };

      greedy2D(X, Z, mask, emitTop, same);
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
          const bool draw = isAir(nb) || (BlockDataManager::getInstance().isOpaque(bt) != BlockDataManager::getInstance().isOpaque(nb));
          if (!draw) {
            mask[x + z * X].set = false;
            continue;
          }
          mask[x + z * X].set = true;
          mask[x + z * X].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::BOTTOM);
        }
      }
      auto emitBottom = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0;
        tileBaseNoPad(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0);
        glm::vec2 tileBase(u0, v0);

        glm::vec3 P[4] = {
            {(float)u, (float)y, (float)v},
            {(float)(u + w), (float)y, (float)v},
            {(float)(u + w), (float)y, (float)(v + h)},
            {(float)u, (float)y, (float)(v + h)},
        };
        // oriented to match your CCW for bottom
        glm::vec2 LUV[4] = {{0, 0}, {w, 0}, {w, h}, {0, h}};

        auto &VV = BlockDataManager::getInstance().isTransparent(sample(baseX + u, y, baseZ + v)) ? vT : vO;
        auto &II = BlockDataManager::getInstance().isTransparent(sample(baseX + u, y, baseZ + v)) ? iT : iO;
        pushQuadTiled(VV, II, P, {0, -1, 0}, LUV, tileBase);
      };
      greedy2D(X, Z, mask, emitBottom, same);
    }
  }

  // -------- ±Z (FRONT/BACK) --------
  {
    mask.assign(X * Y, MaskCell());

    // FRONT (+Z)
    for (int z = 0; z < Z; ++z) {
      for (int y = 0; y < Y; ++y) {
        for (int x = 0; x < X; ++x) {
          const BlockType bt = sample(baseX + x, y, baseZ + z);
          if (isAir(bt)) {
            mask[x + y * X].set = false;
            continue;
          }
          const BlockType nb = edgeFluidNeighbor(bt, x, y, z, /*nx=*/0, /*nz=*/+1);
          const bool draw = isAir(nb) || (BlockDataManager::getInstance().isOpaque(bt) != BlockDataManager::getInstance().isOpaque(nb));
          if (!draw) {
            mask[x + y * X].set = false;
            continue;
          }
          mask[x + y * X].set = true;
          mask[x + y * X].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::FRONT);
        }
      }
      auto emitFront = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0;
        tileBaseNoPad(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0);
        glm::vec2 tileBase(u0, v0);

        glm::vec3 P[4] = {
            {(float)u, (float)v, (float)(z + 1)},
            {(float)(u + w), (float)v, (float)(z + 1)},
            {(float)(u + w), (float)(v + h), (float)(z + 1)},
            {(float)u, (float)(v + h), (float)(z + 1)},
        };
        glm::vec2 LUV[4] = {{0, 0}, {w, 0}, {w, h}, {0, h}};
        auto &VV = BlockDataManager::getInstance().isTransparent(sample(baseX + u, v, baseZ + z)) ? vT : vO;
        auto &II = BlockDataManager::getInstance().isTransparent(sample(baseX + u, v, baseZ + z)) ? iT : iO;
        pushQuadTiled(VV, II, P, {0, 0, 1}, LUV, tileBase);
      };
      greedy2D(X, Y, mask, emitFront, same);
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
          const BlockType nb = edgeFluidNeighbor(bt, x, y, z, /*nx=*/0, /*nz=*/-1);
          const bool draw = isAir(nb) || (BlockDataManager::getInstance().isOpaque(bt) != BlockDataManager::getInstance().isOpaque(nb));
          if (!draw) {
            mask[x + y * X].set = false;
            continue;
          }
          mask[x + y * X].set = true;
          mask[x + y * X].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::BACK);
        }
      }
      auto emitBack = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0;
        tileBaseNoPad(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0);
        glm::vec2 tileBase(u0, v0);

        glm::vec3 P[4] = {
            {(float)(u + w), (float)v, (float)z},
            {(float)u, (float)v, (float)z},
            {(float)u, (float)(v + h), (float)z},
            {(float)(u + w), (float)(v + h), (float)z},
        };
        // flip X to compensate reversed positions
        glm::vec2 LUV[4] = {{w, 0}, {0, 0}, {0, h}, {w, h}};

        auto &VV = BlockDataManager::getInstance().isTransparent(sample(baseX + u, v, baseZ + z)) ? vT : vO;
        auto &II = BlockDataManager::getInstance().isTransparent(sample(baseX + u, v, baseZ + z)) ? iT : iO;
        pushQuadTiled(VV, II, P, {0, 0, -1}, LUV, tileBase);
      };
      greedy2D(X, Y, mask, emitBack, same);
    }
  }

  // -------- ±X (RIGHT/LEFT) --------
  {
    mask.assign(Z * Y, MaskCell()); // U=Z, V=Y

    // RIGHT (+X)
    for (int x = 0; x < X; ++x) {
      for (int y = 0; y < Y; ++y) {
        for (int z = 0; z < Z; ++z) {
          const BlockType bt = sample(baseX + x, y, baseZ + z);
          if (isAir(bt)) {
            mask[z + y * Z].set = false;
            continue;
          }
          const BlockType nb = edgeFluidNeighbor(bt, x, y, z, /*nx=*/+1, /*nz=*/0);
          const bool draw = isAir(nb) || (BlockDataManager::getInstance().isOpaque(bt) != BlockDataManager::getInstance().isOpaque(nb));
          if (!draw) {
            mask[z + y * Z].set = false;
            continue;
          }
          mask[z + y * Z].set = true;
          mask[z + y * Z].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::RIGHT);
        }
      }
      auto emitRight = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0;
        tileBaseNoPad(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0);
        glm::vec2 tileBase(u0, v0);

        glm::vec3 P[4] = {
            {(float)(x + 1), (float)v, (float)(u + w)},       // bl (Z+)
            {(float)(x + 1), (float)v, (float)u},             // br (Z-)
            {(float)(x + 1), (float)(v + h), (float)u},       // tr
            {(float)(x + 1), (float)(v + h), (float)(u + w)}, // tl
        };
        // X+ face increases to the left in Z, so flip X in LUV
        glm::vec2 LUV[4] = {{w, 0}, {0, 0}, {0, h}, {w, h}};

        auto &VV = BlockDataManager::getInstance().isTransparent(sample(baseX + x, v, baseZ + u)) ? vT : vO;
        auto &II = BlockDataManager::getInstance().isTransparent(sample(baseX + x, v, baseZ + u)) ? iT : iO;
        pushQuadTiled(VV, II, P, {1, 0, 0}, LUV, tileBase);
      };
      greedy2D(Z, Y, mask, emitRight, same);
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
          const BlockType nb = edgeFluidNeighbor(bt, x, y, z, /*nx=*/-1, /*nz=*/0);
          const bool draw = isAir(nb) || (BlockDataManager::getInstance().isOpaque(bt) != BlockDataManager::getInstance().isOpaque(nb));
          if (!draw) {
            mask[z + y * Z].set = false;
            continue;
          }
          mask[z + y * Z].set = true;
          mask[z + y * Z].tile =
              BlockDataManager::getInstance().getUV(bt, BlockFace::LEFT);
        }
      }
      auto emitLeft = [&](int u, int v, int w, int h, const MaskCell &cell) {
        float u0, v0;
        tileBaseNoPad(cell.tile.x, cell.tile.y, ATLAS, TILE, u0, v0);
        glm::vec2 tileBase(u0, v0);

        glm::vec3 P[4] = {
            {(float)x, (float)v, (float)u},             // bl (Z-)
            {(float)x, (float)v, (float)(u + w)},       // br (Z+)
            {(float)x, (float)(v + h), (float)(u + w)}, // tr
            {(float)x, (float)(v + h), (float)u},       // tl
        };
        glm::vec2 LUV[4] = {{0, 0}, {w, 0}, {w, h}, {0, h}};

        auto &VV = BlockDataManager::getInstance().isTransparent(sample(baseX + x, v, baseZ + u)) ? vT : vO;
        auto &II = BlockDataManager::getInstance().isTransparent(sample(baseX + x, v, baseZ + u)) ? iT : iO;
        pushQuadTiled(VV, II, P, {-1, 0, 0}, LUV, tileBase);
      };
      greedy2D(Z, Y, mask, emitLeft, same);
    }
  }

  CpuMesh opaqueMesh, transparentMesh;
  if (!iO.empty()) {
    opaqueMesh.vertices = std::move(vO);
    opaqueMesh.indices = std::move(iO);
  }

  if (!iT.empty()) {
    transparentMesh.vertices = std::move(vT);
    transparentMesh.indices = std::move(iT);
  }

  return {opaqueMesh, transparentMesh};
}
