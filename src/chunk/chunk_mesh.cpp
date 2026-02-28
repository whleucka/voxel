#include "chunk/chunk_mesh.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <array>

namespace {

// Helper to pack a float position into int16 (multiply by 2 to preserve 0.5 precision)
inline int16_t packPos(float v) {
  return static_cast<int16_t>(v * 2.0f);
}

struct MeshBuffers {
  std::vector<BlockVertex>& vertices;
  std::vector<unsigned int>& indices;
  std::vector<BlockVertex>& transparentVertices;
  std::vector<unsigned int>& transparentIndices;
};

// Determine if a face should be rendered between current block and neighbor
inline bool shouldRenderFace(BlockType current, BlockType neighbor) {
  if (neighbor == BlockType::AIR) return true;
  if (isTransparent(current)) {
    // Transparent blocks: only render if neighbor is different type
    return neighbor != current;
  }
  // Opaque blocks: render if neighbor is transparent (visible through it)
  return isTransparent(neighbor);
}

// ─── Ambient Occlusion ─────────────────────────────────────────────────

// Returns true if the block at (x,y,z) should occlude for AO purposes.
// Opaque blocks occlude; transparent/air do not.
inline bool isAOSolid(World* world, const Chunk& chunk, int x, int y, int z) {
  BlockType b = ChunkMesh::getBlock(world, chunk, x, y, z);
  return b != BlockType::AIR && !isTransparent(b) && !isLiquid(b);
}

// Computes AO level for a single vertex corner.
// side1, side2 = the two edge-adjacent blocks on the face plane
// corner = the diagonally-adjacent block
// Returns 0 (darkest) to 3 (brightest).
inline int vertexAO(bool side1, bool side2, bool corner) {
  if (side1 && side2) return 0;
  return 3 - (int(side1) + int(side2) + int(corner));
}

// Computes AO for all 4 corners of a face at block position (x,y,z).
// Returns {TL, TR, BR, BL} matching the vertex order in addQuad.
// The corner ordering must match the vertex positions emitted per face.
using AO4 = std::array<uint8_t, 4>;

AO4 computeFaceAO(World* world, const Chunk& chunk, int x, int y, int z, Face face) {
  // For each face, we need to sample 8 neighbors on the face's plane.
  // The face plane is offset by the face normal from (x,y,z).
  // We define two tangent axes (a, b) on the face plane.
  // Neighbors are at: (-a,-b), (0,-b), (+a,-b), (-a,0), (+a,0), (-a,+b), (0,+b), (+a,+b)
  // For each corner, we pick the 2 edges + 1 corner.

  // Lambda to check solidity relative to face
  auto S = [&](int dx, int dy, int dz) -> bool {
    return isAOSolid(world, chunk, x + dx, y + dy, z + dz);
  };

  switch (face) {
    case Face::Top: { // +Y face: plane at y+1, tangent axes = x, z
      // Neighbors in the y+1 plane
      bool xn = S(-1, 1,  0); // -X edge
      bool xp = S( 1, 1,  0); // +X edge
      bool zn = S( 0, 1, -1); // -Z edge
      bool zp = S( 0, 1,  1); // +Z edge
      bool xnzn = S(-1, 1, -1);
      bool xpzn = S( 1, 1, -1);
      bool xpzp = S( 1, 1,  1);
      bool xnzp = S(-1, 1,  1);
      // Vertex order: TL(-x,-z), TR(+x,-z), BR(+x,+z), BL(-x,+z)
      return {{
        static_cast<uint8_t>(vertexAO(xn, zn, xnzn)),  // TL
        static_cast<uint8_t>(vertexAO(xp, zn, xpzn)),  // TR
        static_cast<uint8_t>(vertexAO(xp, zp, xpzp)),  // BR
        static_cast<uint8_t>(vertexAO(xn, zp, xnzp)),  // BL
      }};
    }
    case Face::Bottom: { // -Y face: plane at y-1
      bool xn = S(-1, -1,  0);
      bool xp = S( 1, -1,  0);
      bool zn = S( 0, -1, -1);
      bool zp = S( 0, -1,  1);
      bool xnzn = S(-1, -1, -1);
      bool xpzn = S( 1, -1, -1);
      bool xpzp = S( 1, -1,  1);
      bool xnzp = S(-1, -1,  1);
      // Vertex order: TL(-x,+z), TR(+x,+z), BR(+x,-z), BL(-x,-z)
      return {{
        static_cast<uint8_t>(vertexAO(xn, zp, xnzp)),  // TL
        static_cast<uint8_t>(vertexAO(xp, zp, xpzp)),  // TR
        static_cast<uint8_t>(vertexAO(xp, zn, xpzn)),  // BR
        static_cast<uint8_t>(vertexAO(xn, zn, xnzn)),  // BL
      }};
    }
    case Face::Front: { // +Z face: plane at z+1, tangent axes = x (horiz), y (vert)
      bool xn = S(-1, 0, 1);
      bool xp = S( 1, 0, 1);
      bool yn = S( 0,-1, 1);
      bool yp = S( 0, 1, 1);
      bool xnyp = S(-1, 1, 1);
      bool xpyp = S( 1, 1, 1);
      bool xpyn = S( 1,-1, 1);
      bool xnyn = S(-1,-1, 1);
      // Vertex order: TL(-x,+y), TR(+x,+y), BR(+x,-y), BL(-x,-y)
      return {{
        static_cast<uint8_t>(vertexAO(xn, yp, xnyp)),  // TL
        static_cast<uint8_t>(vertexAO(xp, yp, xpyp)),  // TR
        static_cast<uint8_t>(vertexAO(xp, yn, xpyn)),  // BR
        static_cast<uint8_t>(vertexAO(xn, yn, xnyn)),  // BL
      }};
    }
    case Face::Back: { // -Z face: plane at z-1
      bool xn = S(-1, 0, -1);
      bool xp = S( 1, 0, -1);
      bool yn = S( 0,-1, -1);
      bool yp = S( 0, 1, -1);
      bool xnyp = S(-1, 1, -1);
      bool xpyp = S( 1, 1, -1);
      bool xpyn = S( 1,-1, -1);
      bool xnyn = S(-1,-1, -1);
      // Vertex order: TL(+x,+y), TR(-x,+y), BR(-x,-y), BL(+x,-y)
      return {{
        static_cast<uint8_t>(vertexAO(xp, yp, xpyp)),  // TL
        static_cast<uint8_t>(vertexAO(xn, yp, xnyp)),  // TR
        static_cast<uint8_t>(vertexAO(xn, yn, xnyn)),  // BR
        static_cast<uint8_t>(vertexAO(xp, yn, xpyn)),  // BL
      }};
    }
    case Face::Right: { // +X face: plane at x+1, tangent axes = z (horiz), y (vert)
      bool zn = S(1, 0, -1);
      bool zp = S(1, 0,  1);
      bool yn = S(1,-1,  0);
      bool yp = S(1, 1,  0);
      bool znyp = S(1, 1, -1);
      bool zpyp = S(1, 1,  1);
      bool zpyn = S(1,-1,  1);
      bool znyn = S(1,-1, -1);
      // Vertex order: TL(-z,+y), TR(+z,+y), BR(+z,-y), BL(-z,-y)
      return {{
        static_cast<uint8_t>(vertexAO(zn, yp, znyp)),  // TL
        static_cast<uint8_t>(vertexAO(zp, yp, zpyp)),  // TR
        static_cast<uint8_t>(vertexAO(zp, yn, zpyn)),  // BR
        static_cast<uint8_t>(vertexAO(zn, yn, znyn)),  // BL
      }};
    }
    case Face::Left: { // -X face: plane at x-1
      bool zn = S(-1, 0, -1);
      bool zp = S(-1, 0,  1);
      bool yn = S(-1,-1,  0);
      bool yp = S(-1, 1,  0);
      bool znyp = S(-1, 1, -1);
      bool zpyp = S(-1, 1,  1);
      bool zpyn = S(-1,-1,  1);
      bool znyn = S(-1,-1, -1);
      // Vertex order: TL(+z,+y), TR(-z,+y), BR(-z,-y), BL(+z,-y)
      return {{
        static_cast<uint8_t>(vertexAO(zp, yp, zpyp)),  // TL
        static_cast<uint8_t>(vertexAO(zn, yp, znyp)),  // TR
        static_cast<uint8_t>(vertexAO(zn, yn, znyn)),  // BR
        static_cast<uint8_t>(vertexAO(zp, yn, zpyn)),  // BL
      }};
    }
  }
  return {{3, 3, 3, 3}}; // fallback: no occlusion
}

void addQuad(MeshBuffers& buffers,
             int posX, int posY, int posZ,
             int sizeA, int sizeB,
             int tileX, int tileY,
             Face face,
             int16_t chunkWorldX, int16_t chunkWorldZ,
             bool transparent,
             const AO4& ao) {
  auto& vertices = transparent ? buffers.transparentVertices : buffers.vertices;
  auto& indices = transparent ? buffers.transparentIndices : buffers.indices;

  unsigned int start_index = static_cast<unsigned int>(vertices.size());
  uint8_t faceId = static_cast<uint8_t>(face);

  // Lambda to add a vertex with packed data
  auto V = [&](float px, float py, float pz, uint8_t uvX, uint8_t uvY, uint8_t aoVal) {
    vertices.push_back({
      packPos(px), packPos(py), packPos(pz),
      faceId,
      static_cast<uint8_t>(tileX), static_cast<uint8_t>(tileY),
      uvX, uvY,
      aoVal,
      chunkWorldX, chunkWorldZ
    });
  };

  switch (face) {
  case Face::Top:                                                           // +Y
    V(posX - 0.5f, posY + 0.5f, posZ - 0.5f,              0, 0,       ao[0]); // TL
    V(posX + sizeA - 0.5f, posY + 0.5f, posZ - 0.5f,      sizeA, 0,   ao[1]); // TR
    V(posX + sizeA - 0.5f, posY + 0.5f, posZ + sizeB - 0.5f, sizeA, sizeB, ao[2]); // BR
    V(posX - 0.5f, posY + 0.5f, posZ + sizeB - 0.5f,      0, sizeB,  ao[3]); // BL
    break;

  case Face::Bottom:                                                        // -Y
    V(posX - 0.5f, posY - 0.5f, posZ + sizeB - 0.5f,      0, 0,       ao[0]); // TL
    V(posX + sizeA - 0.5f, posY - 0.5f, posZ + sizeB - 0.5f, sizeA, 0, ao[1]); // TR
    V(posX + sizeA - 0.5f, posY - 0.5f, posZ - 0.5f,      sizeA, sizeB, ao[2]); // BR
    V(posX - 0.5f, posY - 0.5f, posZ - 0.5f,              0, sizeB,   ao[3]); // BL
    break;

  case Face::Front:                                                         // +Z
    V(posX - 0.5f, posY + sizeB - 0.5f, posZ + 0.5f,      0, 0,       ao[0]); // TL
    V(posX + sizeA - 0.5f, posY + sizeB - 0.5f, posZ + 0.5f, sizeA, 0, ao[1]); // TR
    V(posX + sizeA - 0.5f, posY - 0.5f, posZ + 0.5f,      sizeA, sizeB, ao[2]); // BR
    V(posX - 0.5f, posY - 0.5f, posZ + 0.5f,              0, sizeB,   ao[3]); // BL
    break;

  case Face::Back:                                                          // -Z
    V(posX + sizeA - 0.5f, posY + sizeB - 0.5f, posZ - 0.5f, 0, 0,    ao[0]); // TL
    V(posX - 0.5f, posY + sizeB - 0.5f, posZ - 0.5f,      sizeA, 0,   ao[1]); // TR
    V(posX - 0.5f, posY - 0.5f, posZ - 0.5f,              sizeA, sizeB, ao[2]); // BR
    V(posX + sizeA - 0.5f, posY - 0.5f, posZ - 0.5f,      0, sizeB,   ao[3]); // BL
    break;

  case Face::Right:                                                         // +X
    V(posX + 0.5f, posY + sizeB - 0.5f, posZ - 0.5f,      0, 0,       ao[0]); // TL
    V(posX + 0.5f, posY + sizeB - 0.5f, posZ + sizeA - 0.5f, sizeA, 0, ao[1]); // TR
    V(posX + 0.5f, posY - 0.5f, posZ + sizeA - 0.5f,      sizeA, sizeB, ao[2]); // BR
    V(posX + 0.5f, posY - 0.5f, posZ - 0.5f,              0, sizeB,   ao[3]); // BL
    break;

  case Face::Left:                                                          // -X
    V(posX - 0.5f, posY + sizeB - 0.5f, posZ + sizeA - 0.5f, 0, 0,    ao[0]); // TL
    V(posX - 0.5f, posY + sizeB - 0.5f, posZ - 0.5f,      sizeA, 0,   ao[1]); // TR
    V(posX - 0.5f, posY - 0.5f, posZ - 0.5f,              sizeA, sizeB, ao[2]); // BR
    V(posX - 0.5f, posY - 0.5f, posZ + sizeA - 0.5f,      0, sizeB,   ao[3]); // BL
    break;
  }

  // Quad flip: choose the triangle diagonal that avoids AO interpolation artifacts.
  // When AO values differ across opposite corners, we flip the diagonal so the
  // brighter pair shares the triangle edge, preventing a dark seam artifact.
  bool flip = (ao[0] + ao[2]) < (ao[1] + ao[3]);

  // The base winding order differs between face groups to maintain CCW front-facing.
  // Left/Right faces use a different vertex arrangement, so their index order differs.
  switch (face) {
  case Face::Top:
  case Face::Bottom:
  case Face::Front:
  case Face::Back:
    if (!flip) {
      indices.push_back(start_index + 0);
      indices.push_back(start_index + 3);
      indices.push_back(start_index + 2);
      indices.push_back(start_index + 2);
      indices.push_back(start_index + 1);
      indices.push_back(start_index + 0);
    } else {
      indices.push_back(start_index + 1);
      indices.push_back(start_index + 0);
      indices.push_back(start_index + 3);
      indices.push_back(start_index + 3);
      indices.push_back(start_index + 2);
      indices.push_back(start_index + 1);
    }
    break;

  case Face::Left:
  case Face::Right:
    if (!flip) {
      indices.push_back(start_index + 0);
      indices.push_back(start_index + 1);
      indices.push_back(start_index + 2);
      indices.push_back(start_index + 2);
      indices.push_back(start_index + 3);
      indices.push_back(start_index + 0);
    } else {
      indices.push_back(start_index + 3);
      indices.push_back(start_index + 0);
      indices.push_back(start_index + 1);
      indices.push_back(start_index + 1);
      indices.push_back(start_index + 2);
      indices.push_back(start_index + 3);
    }
    break;
  }
}

} // namespace

ChunkMesh::ChunkMesh() {
  // GL objects created lazily in upload() on main thread
}

ChunkMesh::~ChunkMesh() {
  if (VAO) glDeleteVertexArrays(1, &VAO);
  if (VBO) glDeleteBuffers(1, &VBO);
  if (EBO) glDeleteBuffers(1, &EBO);
  if (transparentVAO) glDeleteVertexArrays(1, &transparentVAO);
  if (transparentVBO) glDeleteBuffers(1, &transparentVBO);
  if (transparentEBO) glDeleteBuffers(1, &transparentEBO);
}

BlockType ChunkMesh::getBlock(World *world, const Chunk &chunk, int x, int y, int z) {
  if (y < 0 || y >= kChunkHeight) {
    return BlockType::AIR;
  }

  const int chunk_x = chunk.getPos()[0];
  const int chunk_z = chunk.getPos()[1];

  const int world_x = chunk_x * kChunkWidth + x;
  const int world_z = chunk_z * kChunkDepth + z;

  const int target_chunk_x = floor((float)world_x / kChunkWidth);
  const int target_chunk_z = floor((float)world_z / kChunkDepth);

  if (target_chunk_x != chunk_x || target_chunk_z != chunk_z) {
    auto n = world->getChunk(target_chunk_x, target_chunk_z);
    const int local_x = world_x - target_chunk_x * kChunkWidth;
    const int local_z = world_z - target_chunk_z * kChunkDepth;
    return n ? n->at(local_x, y, local_z) : BlockType::AIR;
  }

  return chunk.at(x, y, z);
}

void ChunkMesh::generateCPU(World *world, const Chunk &chunk,
                            TextureManager &) {
  cpuReady = false;
  // gpuUploaded intentionally NOT reset — old GPU buffers keep rendering
  // while this CPU rebuild runs; upload() will overwrite them in-place.
  vertices.clear();
  indices.clear();
  transparentVertices.clear();
  transparentIndices.clear();

  // Compute chunk world offset for batch rendering
  const int16_t chunkWorldX = static_cast<int16_t>(chunk.getPos().x * kChunkWidth);
  const int16_t chunkWorldZ = static_cast<int16_t>(chunk.getPos().y * kChunkDepth);

  MeshBuffers buffers{vertices, indices, transparentVertices, transparentIndices};

  // Front face (+Z)
  for (int z = 0; z < kChunkDepth; ++z) {
    bool mask[kChunkWidth][kChunkHeight] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[x][y]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue;

        BlockType neighbor = getBlock(world, chunk, x, y, z + 1);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Front);

          int width = 1;
          while (x + width < kChunkWidth && !mask[x + width][y] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y, z + 1)) &&
                 computeFaceAO(world, chunk, x + width, y, z, Face::Front) == ao) {
            width++;
          }

          int height = 1;
          bool can_expand = true;
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[x + i][y + height] ||
                  chunk.safeAt(x + i, y + height, z) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + i, y + height, z + 1)) ||
                  computeFaceAO(world, chunk, x + i, y + height, z, Face::Front) != ao) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) height++;
          }

          for (int i = 0; i < height; ++i)
            for (int j = 0; j < width; ++j)
              mask[x + j][y + i] = true;

          addQuad(buffers, x, y, z, width, height,
                  tex.side.x, tex.side.y, Face::Front, chunkWorldX, chunkWorldZ,
                  isTransparent(type), ao);
        }
      }
    }
  }

  // Back face (-Z)
  for (int z = kChunkDepth - 1; z >= 0; --z) {
    bool mask[kChunkWidth][kChunkHeight] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[x][y]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue;

        BlockType neighbor = getBlock(world, chunk, x, y, z - 1);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Back);

          int width = 1;
          while (x + width < kChunkWidth && !mask[x + width][y] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y, z - 1)) &&
                 computeFaceAO(world, chunk, x + width, y, z, Face::Back) == ao) {
            width++;
          }

          int height = 1;
          bool can_expand = true;
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[x + i][y + height] ||
                  chunk.safeAt(x + i, y + height, z) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + i, y + height, z - 1)) ||
                  computeFaceAO(world, chunk, x + i, y + height, z, Face::Back) != ao) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) height++;
          }

          for (int i = 0; i < height; ++i)
            for (int j = 0; j < width; ++j)
              mask[x + j][y + i] = true;

          addQuad(buffers, x, y, z, width, height,
                  tex.side.x, tex.side.y, Face::Back, chunkWorldX, chunkWorldZ,
                  isTransparent(type), ao);
        }
      }
    }
  }

  // Top face (+Y)
  for (int y = 0; y < kChunkHeight; ++y) {
    bool mask[kChunkDepth][kChunkWidth] = {false};
    for (int z = 0; z < kChunkDepth; ++z) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[z][x]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;

        BlockType neighbor = getBlock(world, chunk, x, y + 1, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Top);

          int width = 1;
          while (x + width < kChunkWidth && !mask[z][x + width] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y + 1, z)) &&
                 computeFaceAO(world, chunk, x + width, y, z, Face::Top) == ao) {
            width++;
          }

          int depth = 1;
          bool can_expand = true;
          while (z + depth < kChunkDepth && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[z + depth][x + i] ||
                  chunk.safeAt(x + i, y, z + depth) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + i, y + 1, z + depth)) ||
                  computeFaceAO(world, chunk, x + i, y, z + depth, Face::Top) != ao) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) depth++;
          }

          for (int i = 0; i < depth; ++i)
            for (int j = 0; j < width; ++j)
              mask[z + i][x + j] = true;

          addQuad(buffers, x, y, z, width, depth,
                  tex.top.x, tex.top.y, Face::Top, chunkWorldX, chunkWorldZ,
                  isTransparent(type), ao);

          // For water at the surface (air above), also render bottom face
          // so it's visible from underwater looking up
          if (isLiquid(type) && neighbor == BlockType::AIR) {
            AO4 noAO = {{3, 3, 3, 3}};
            addQuad(buffers, x, y + 1, z, width, depth,
                    tex.bottom.x, tex.bottom.y, Face::Bottom, chunkWorldX, chunkWorldZ,
                    true, noAO);
          }
        }
      }
    }
  }

  // Bottom face (-Y)
  for (int y = kChunkHeight - 1; y >= 0; --y) {
    bool mask[kChunkDepth][kChunkWidth] = {false};
    for (int z = 0; z < kChunkDepth; ++z) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[z][x]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue;

        BlockType neighbor = getBlock(world, chunk, x, y - 1, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Bottom);

          int width = 1;
          while (x + width < kChunkWidth && !mask[z][x + width] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y - 1, z)) &&
                 computeFaceAO(world, chunk, x + width, y, z, Face::Bottom) == ao) {
            width++;
          }

          int depth = 1;
          bool can_expand = true;
          while (z + depth < kChunkDepth && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[z + depth][x + i] ||
                  chunk.safeAt(x + i, y, z + depth) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + i, y - 1, z + depth)) ||
                  computeFaceAO(world, chunk, x + i, y, z + depth, Face::Bottom) != ao) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) depth++;
          }

          for (int i = 0; i < depth; ++i)
            for (int j = 0; j < width; ++j)
              mask[z + i][x + j] = true;

          addQuad(buffers, x, y, z, width, depth,
                  tex.bottom.x, tex.bottom.y, Face::Bottom, chunkWorldX, chunkWorldZ,
                  isTransparent(type), ao);
        }
      }
    }
  }

  // Right face (+X)
  for (int x = 0; x < kChunkWidth; ++x) {
    bool mask[kChunkHeight][kChunkDepth] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int z = 0; z < kChunkDepth; ++z) {
        if (mask[y][z]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue;

        BlockType neighbor = getBlock(world, chunk, x + 1, y, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Right);

          int depth = 1;
          while (z + depth < kChunkDepth && !mask[y][z + depth] &&
                 chunk.safeAt(x, y, z + depth) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + 1, y, z + depth)) &&
                 computeFaceAO(world, chunk, x, y, z + depth, Face::Right) == ao) {
            depth++;
          }

          int height = 1;
          bool can_expand = true;
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < depth; ++i) {
              if (mask[y + height][z + i] ||
                  chunk.safeAt(x, y + height, z + i) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + 1, y + height, z + i)) ||
                  computeFaceAO(world, chunk, x, y + height, z + i, Face::Right) != ao) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) height++;
          }

          for (int i = 0; i < height; ++i)
            for (int j = 0; j < depth; ++j)
              mask[y + i][z + j] = true;

          addQuad(buffers, x, y, z, depth, height,
                  tex.side.x, tex.side.y, Face::Right, chunkWorldX, chunkWorldZ,
                  isTransparent(type), ao);
        }
      }
    }
  }

  // Left face (-X)
  for (int x = kChunkWidth - 1; x >= 0; --x) {
    bool mask[kChunkHeight][kChunkDepth] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int z = 0; z < kChunkDepth; ++z) {
        if (mask[y][z]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue;

        BlockType neighbor = getBlock(world, chunk, x - 1, y, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Left);

          int depth = 1;
          while (z + depth < kChunkDepth && !mask[y][z + depth] &&
                 chunk.safeAt(x, y, z + depth) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x - 1, y, z + depth)) &&
                 computeFaceAO(world, chunk, x, y, z + depth, Face::Left) == ao) {
            depth++;
          }

          int height = 1;
          bool can_expand = true;
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < depth; ++i) {
              if (mask[y + height][z + i] ||
                  chunk.safeAt(x, y + height, z + i) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x - 1, y + height, z + i)) ||
                  computeFaceAO(world, chunk, x, y + height, z + i, Face::Left) != ao) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) height++;
          }

          for (int i = 0; i < height; ++i)
            for (int j = 0; j < depth; ++j)
              mask[y + i][z + j] = true;

          addQuad(buffers, x, y, z, depth, height,
                  tex.side.x, tex.side.y, Face::Left, chunkWorldX, chunkWorldZ,
                  isTransparent(type), ao);
        }
      }
    }
  }

  cpuReady = !vertices.empty() || !transparentVertices.empty();
}

void ChunkMesh::setupVAO(GLuint vao, GLuint vbo, GLuint ebo,
                          const std::vector<BlockVertex>& verts,
                          const std::vector<unsigned int>& inds) {
  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(BlockVertex),
               verts.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int),
               inds.data(), GL_STATIC_DRAW);

  // Position: 3 x int16 at offset 0
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(BlockVertex),
                        (void *)0);

  // FaceId: uint8 at offset 6
  glEnableVertexAttribArray(1);
  glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, faceId));

  // TileXY: 2 x uint8 at offset 7
  glEnableVertexAttribArray(2);
  glVertexAttribIPointer(2, 2, GL_UNSIGNED_BYTE, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, tileX));

  // UV: 2 x uint8 at offset 9
  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(3, 2, GL_UNSIGNED_BYTE, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, uvX));

  // ChunkOffset: 2 x int16 at offset 12
  glEnableVertexAttribArray(4);
  glVertexAttribIPointer(4, 2, GL_SHORT, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, chunkX));

  // AO: uint8 at offset 11
  glEnableVertexAttribArray(5);
  glVertexAttribIPointer(5, 1, GL_UNSIGNED_BYTE, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, ao));

  glBindVertexArray(0);
}

void ChunkMesh::upload() {
  if (!cpuReady)
    return;

  if (!vertices.empty()) {
    if (!VAO) {
      glGenVertexArrays(1, &VAO);
      glGenBuffers(1, &VBO);
      glGenBuffers(1, &EBO);
    }
    setupVAO(VAO, VBO, EBO, vertices, indices);
  }

  if (!transparentVertices.empty()) {
    if (!transparentVAO) {
      glGenVertexArrays(1, &transparentVAO);
      glGenBuffers(1, &transparentVBO);
      glGenBuffers(1, &transparentEBO);
    }
    setupVAO(transparentVAO, transparentVBO, transparentEBO,
             transparentVertices, transparentIndices);
  }

  // Snapshot counts before marking GPU data as live.
  // These are the only values the render methods will ever read from the CPU
  // side — the raw vectors may be cleared/rebuilt by the mesh thread at any
  // time after this point.
  opaque_index_count      = static_cast<GLsizei>(indices.size());
  transparent_index_count = static_cast<GLsizei>(transparentIndices.size());
  gpuUploaded = true;
}

void ChunkMesh::renderOpaque() {
  if (!gpuUploaded || opaque_index_count == 0)
    return;

  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, opaque_index_count, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void ChunkMesh::renderTransparent() {
  if (!gpuUploaded || transparent_index_count == 0)
    return;

  glBindVertexArray(transparentVAO);
  glDrawElements(GL_TRIANGLES, transparent_index_count, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}
