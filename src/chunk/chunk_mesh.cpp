#include "chunk/chunk_mesh.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <array>
#include <cmath>

namespace {

// Packs a local position into int16, to 1/16 of a block.
inline int16_t packPos(float v) {
  return static_cast<int16_t>(std::lround(v * kPosScale));
}

struct MeshBuffers {
  std::vector<BlockVertex>& vertices;
  std::vector<unsigned int>& indices;
  std::vector<BlockVertex>& transparentVertices;
  std::vector<unsigned int>& transparentIndices;
};

// ─── Ambient Occlusion ─────────────────────────────────────────────────

// Returns true if the block at (x,y,z) should occlude for AO purposes.
// Opaque blocks occlude; transparent/air do not.
inline bool isAOSolid(World* world, const Chunk& chunk, int x, int y, int z) {
  BlockType b = ChunkMesh::getBlock(world, chunk, x, y, z);
  return b != BlockType::AIR && !isTransparent(b) && !isLiquid(b) &&
         !isCutout(b);
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

// A glow crystal grows away from the rock it's on: one with something solid
// above it and nothing below hangs from a ceiling, so its side faces show the
// shard tile upside down. Anything else points up.
bool crystalHangs(World* world, const Chunk& chunk, BlockType type, int x, int y, int z) {
  return type == BlockType::GLOW_CRYSTAL &&
         blocksMovement(ChunkMesh::getBlock(world, chunk, x, y + 1, z)) &&
         !blocksMovement(ChunkMesh::getBlock(world, chunk, x, y - 1, z));
}

// Whether a block's side faces may merge with their neighbours'. A glow
// crystal's can't: a hanging one's face and a standing one's face differ.
bool mergesSides(BlockType type) { return type != BlockType::GLOW_CRYSTAL; }

void addQuad(MeshBuffers& buffers,
             int posX, int posY, int posZ,
             int sizeA, int sizeB,
             int tileX, int tileY,
             Face face,
             int16_t chunkWorldX, int16_t chunkWorldZ,
             bool transparent,
             uint8_t cutoutClass,
             const AO4& ao,
             uint8_t skyLight,
             uint8_t blockLight,
             bool flipV = false) {
  auto& vertices = transparent ? buffers.transparentVertices : buffers.vertices;
  auto& indices = transparent ? buffers.transparentIndices : buffers.indices;

  unsigned int start_index = static_cast<unsigned int>(vertices.size());
  // Pack block light (0-15) into the high bits of the faceId byte; faceId only
  // needs 3 bits (0-5), so bits[6:3] carry block light. Shader unpacks both.
  uint8_t faceId = static_cast<uint8_t>(static_cast<uint8_t>(face) |
                                        ((blockLight & 0xF) << 3));

  // Lambda to add a vertex with packed data.
  // ao byte packs: bits[1:0] = AO level (0-3), bits[5:2] = sky light (0-15),
  // bits[7:6] = cutout class.  The class travels per-vertex rather than as a
  // uniform because each leaf tile needs its own alpha threshold, and because a
  // global test would erase water (a uniform alpha 153 tile).
  const uint8_t cutoutBits = static_cast<uint8_t>((cutoutClass & 0x3) << 6);

  auto V = [&](float px, float py, float pz, uint8_t uvX, uint8_t uvY, uint8_t aoVal) {
    vertices.push_back({
      packPos(px), packPos(py), packPos(pz),
      faceId,
      static_cast<uint8_t>(tileX), static_cast<uint8_t>(tileY),
      uvX, static_cast<uint8_t>(flipV ? sizeB - uvY : uvY), // flipV: tile upside down
      static_cast<uint8_t>(cutoutBits | ((skyLight & 0xF) << 2) | (aoVal & 0x3)),
      chunkWorldX, chunkWorldZ
    });
  };

  // Blocks occupy [posX, posX+1] x [posY, posY+1] x [posZ, posZ+1] in world space,
  // matching the data/physics coordinate system exactly.
  switch (face) {
  case Face::Top:                                                           // +Y  (y = posY+1)
    V(posX,         posY + 1, posZ,         0, 0,         ao[0]); // TL
    V(posX + sizeA, posY + 1, posZ,         sizeA, 0,     ao[1]); // TR
    V(posX + sizeA, posY + 1, posZ + sizeB, sizeA, sizeB, ao[2]); // BR
    V(posX,         posY + 1, posZ + sizeB, 0, sizeB,     ao[3]); // BL
    break;

  case Face::Bottom:                                                        // -Y  (y = posY)
    V(posX,         posY, posZ + sizeB, 0, 0,         ao[0]); // TL
    V(posX + sizeA, posY, posZ + sizeB, sizeA, 0,     ao[1]); // TR
    V(posX + sizeA, posY, posZ,         sizeA, sizeB, ao[2]); // BR
    V(posX,         posY, posZ,         0, sizeB,     ao[3]); // BL
    break;

  case Face::Front:                                                         // +Z  (z = posZ+1)
    V(posX,         posY + sizeB, posZ + 1, 0, 0,         ao[0]); // TL
    V(posX + sizeA, posY + sizeB, posZ + 1, sizeA, 0,     ao[1]); // TR
    V(posX + sizeA, posY,         posZ + 1, sizeA, sizeB, ao[2]); // BR
    V(posX,         posY,         posZ + 1, 0, sizeB,     ao[3]); // BL
    break;

  case Face::Back:                                                          // -Z  (z = posZ)
    V(posX + sizeA, posY + sizeB, posZ, 0, 0,         ao[0]); // TL
    V(posX,         posY + sizeB, posZ, sizeA, 0,     ao[1]); // TR
    V(posX,         posY,         posZ, sizeA, sizeB, ao[2]); // BR
    V(posX + sizeA, posY,         posZ, 0, sizeB,     ao[3]); // BL
    break;

  case Face::Right:                                                         // +X  (x = posX+1)
    V(posX + 1, posY + sizeB, posZ,         0, 0,         ao[0]); // TL
    V(posX + 1, posY + sizeB, posZ + sizeA, sizeA, 0,     ao[1]); // TR
    V(posX + 1, posY,         posZ + sizeA, sizeA, sizeB, ao[2]); // BR
    V(posX + 1, posY,         posZ,         0, sizeB,     ao[3]); // BL
    break;

  case Face::Left:                                                          // -X  (x = posX)
    V(posX, posY + sizeB, posZ + sizeA, 0, 0,         ao[0]); // TL
    V(posX, posY + sizeB, posZ,         sizeA, 0,     ao[1]); // TR
    V(posX, posY,         posZ,         sizeA, sizeB, ao[2]); // BR
    V(posX, posY,         posZ + sizeA, 0, sizeB,     ao[3]); // BL
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

// The tiles a box wears on each face.
struct BoxTiles {
  glm::ivec2 top, bottom, front, back, right, left;
};

// A box inside a cell (doors, beds): all six faces, lit by the cell itself
// (light gets into the cell, since these don't fill it) and without AO.
// Corners go in the order addQuad uses for each face, so the winding matches.
void addBox(MeshBuffers &buffers, glm::vec3 lo, glm::vec3 hi, const BoxTiles &tiles,
            uint8_t cutout, int16_t chunkWorldX, int16_t chunkWorldZ, uint8_t skyLight,
            uint8_t blockLight) {
  const uint8_t cutoutBits = static_cast<uint8_t>((cutout & 0x3) << 6);
  const uint8_t light = static_cast<uint8_t>(cutoutBits | ((skyLight & 0xF) << 2) | 3);

  auto face = [&](Face f, glm::ivec2 tile, const std::array<glm::vec3, 4> &corners) {
    unsigned int start = static_cast<unsigned int>(buffers.vertices.size());
    const uint8_t faceId =
        static_cast<uint8_t>(static_cast<uint8_t>(f) | ((blockLight & 0xF) << 3));
    const uint8_t uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
      buffers.vertices.push_back({packPos(corners[i].x), packPos(corners[i].y),
                                  packPos(corners[i].z), faceId,
                                  static_cast<uint8_t>(tile.x), static_cast<uint8_t>(tile.y),
                                  uv[i][0], uv[i][1], light, chunkWorldX, chunkWorldZ});
    const bool sideways = f == Face::Left || f == Face::Right;
    const unsigned int order[2][6] = {{0, 3, 2, 2, 1, 0}, {0, 1, 2, 2, 3, 0}};
    for (unsigned int k : order[sideways]) buffers.indices.push_back(start + k);
  };

  face(Face::Top, tiles.top,
       {{{lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}}});
  face(Face::Bottom, tiles.bottom,
       {{{lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, lo.y, lo.z}, {lo.x, lo.y, lo.z}}});
  face(Face::Front, tiles.front,
       {{{lo.x, hi.y, hi.z}, {hi.x, hi.y, hi.z}, {hi.x, lo.y, hi.z}, {lo.x, lo.y, hi.z}}});
  face(Face::Back, tiles.back,
       {{{hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z}, {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}}});
  face(Face::Right, tiles.right,
       {{{hi.x, hi.y, lo.z}, {hi.x, hi.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, lo.y, lo.z}}});
  face(Face::Left, tiles.left,
       {{{lo.x, hi.y, hi.z}, {lo.x, hi.y, lo.z}, {lo.x, lo.y, lo.z}, {lo.x, lo.y, hi.z}}});
}

// A door: its panel, a thin box. The two big faces wear the half's tile; the
// thin edges, the edge tile.
void addDoor(MeshBuffers &buffers, int x, int y, int z, BlockType type,
             int16_t chunkWorldX, int16_t chunkWorldZ, uint8_t skyLight, uint8_t blockLight) {
  const DoorBox box = doorPanel(type);
  const glm::vec3 lo = glm::vec3(x, y, z) + box.lo, hi = glm::vec3(x, y, z) + box.hi;
  const BlockTexture &tex = block_data.at(type);
  const glm::vec3 size = hi - lo;
  auto tileFor = [&](float w) { return w > 0.5f ? tex.side : tex.top; };
  const glm::ivec2 xs = tileFor(size.x), zs = tileFor(size.z);
  addBox(buffers, lo, hi, {tex.top, tex.bottom, xs, xs, zs, zs}, cutoutClass(type),
         chunkWorldX, chunkWorldZ, skyLight, blockLight);
}

// A bed half: a wooden frame with a blanket on it, and at the head a pillow
// at the far end. The boxes are laid out for a bed facing north (head at
// -Z) and turned to the bed's facing.
void addBed(MeshBuffers &buffers, int x, int y, int z, BlockType type,
            int16_t chunkWorldX, int16_t chunkWorldZ, uint8_t skyLight, uint8_t blockLight) {
  constexpr float s = 1.0f / 16.0f;
  constexpr float frame_top = 5 * s;
  const glm::ivec2 frame = kBedFrameTile, pillow = kBedPillowTile;
  const glm::ivec2 blanket = bedBlanketTile(bedColour(type));
  const DoorFacing facing = bedFacing(type);

  // (x, z) in a north-facing cell to the same point in this bed's cell.
  auto turn = [&](float u, float v) -> glm::vec2 {
    switch (facing) {
      case DoorFacing::North: return {u, v};
      case DoorFacing::South: return {1 - u, 1 - v};
      case DoorFacing::East:  return {1 - v, u};
      case DoorFacing::West:
      default:                return {v, 1 - u};
    }
  };
  auto box = [&](glm::vec3 lo, glm::vec3 hi, const BoxTiles &tiles) {
    glm::vec2 a = turn(lo.x, lo.z), b = turn(hi.x, hi.z);
    glm::vec3 cell(x, y, z);
    addBox(buffers, cell + glm::vec3(std::min(a.x, b.x), lo.y, std::min(a.y, b.y)),
           cell + glm::vec3(std::max(a.x, b.x), hi.y, std::max(a.y, b.y)), tiles, kCutoutNone,
           chunkWorldX, chunkWorldZ, skyLight, blockLight);
  };
  const BoxTiles wood{frame, frame, frame, frame, frame, frame};
  const BoxTiles cover{blanket, frame, blanket, blanket, blanket, blanket};
  const BoxTiles plump{pillow, pillow, pillow, pillow, pillow, pillow};

  box({0, 0, 0}, {1, frame_top, 1}, wood);
  if (!bedHead(type)) {
    box({0, frame_top, 0}, {1, kBedHeight, 1}, cover);
  } else {
    box({0, frame_top, 8 * s}, {1, kBedHeight, 1}, cover);
    box({s, frame_top, s}, {1 - s, kBedHeight, 8 * s}, plump);
  }
}

// A crop or a wild plant: four flat planes in a # across the cell, so plants
// side by side never share a face and a field can be seen into. Each plane is
// a box with no thickness: its front and back faces are the plant seen from
// either side.
void addCrop(MeshBuffers &buffers, int x, int y, int z, BlockType type,
             int16_t chunkWorldX, int16_t chunkWorldZ, uint8_t skyLight, uint8_t blockLight) {
  const glm::ivec2 plant = block_data.at(type).side;
  const BoxTiles tiles{kCropTopTile, kCropTopTile, plant, plant, plant, plant};
  const glm::vec3 cell(x, y, z);
  for (float at : {4.0f / 16.0f, 12.0f / 16.0f}) {
    addBox(buffers, cell + glm::vec3(0, 0, at), cell + glm::vec3(1, 1, at), tiles,
           cutoutClass(type), chunkWorldX, chunkWorldZ, skyLight, blockLight);
    addBox(buffers, cell + glm::vec3(at, 0, 0), cell + glm::vec3(at, 1, 1), tiles,
           cutoutClass(type), chunkWorldX, chunkWorldZ, skyLight, blockLight);
  }
}

// A torch: two flat planes crossed through the middle of the cell, wearing
// the stick-and-flame tile. One on a wall stands against it, a little higher.
void addTorch(MeshBuffers &buffers, int x, int y, int z, BlockType type,
              int16_t chunkWorldX, int16_t chunkWorldZ, uint8_t skyLight, uint8_t blockLight) {
  const glm::ivec2 tile = block_data.at(type).side;
  const BoxTiles tiles{kCropTopTile, kCropTopTile, tile, tile, tile, tile};
  const glm::ivec3 wall = torchSupport(type);
  glm::vec3 base(x, y, z);
  if (wall.y == 0) base += glm::vec3(wall) * 0.35f + glm::vec3(0.0f, 0.2f, 0.0f);
  addBox(buffers, base + glm::vec3(0, 0, 0.5f), base + glm::vec3(1, 1, 0.5f), tiles,
         cutoutClass(type), chunkWorldX, chunkWorldZ, skyLight, blockLight);
  addBox(buffers, base + glm::vec3(0.5f, 0, 0), base + glm::vec3(0.5f, 1, 1), tiles,
         cutoutClass(type), chunkWorldX, chunkWorldZ, skyLight, blockLight);
}

// A cactus: a box a little inside its cell, so a stack of them shows ridges.
void addCactus(MeshBuffers &buffers, int x, int y, int z, BlockType type,
               int16_t chunkWorldX, int16_t chunkWorldZ, uint8_t skyLight, uint8_t blockLight) {
  constexpr float s = 1.0f / 16.0f;
  const BlockTexture &tex = block_data.at(type);
  const glm::vec3 cell(x, y, z);
  addBox(buffers, cell + glm::vec3(s, 0, s), cell + glm::vec3(1 - s, 1, 1 - s),
         {tex.top, tex.top, tex.side, tex.side, tex.side, tex.side}, kCutoutNone, chunkWorldX,
         chunkWorldZ, skyLight, blockLight);
}

// A cake: a low round of sponge, with the eaten slices gone from its +X
// end, which shows the inside where it's been cut.
void addCake(MeshBuffers &buffers, int x, int y, int z, BlockType type,
             int16_t chunkWorldX, int16_t chunkWorldZ, uint8_t skyLight, uint8_t blockLight) {
  constexpr float s = 1.0f / 16.0f;
  const BlockTexture &tex = block_data.at(type); // bottom holds the cut
  const int slices = cakeSlices(type);
  const float end = s + 14 * s * static_cast<float>(slices) / kCakeSlices;
  const glm::ivec2 cut = slices < kCakeSlices ? tex.bottom : tex.side;
  const glm::vec3 cell(x, y, z);
  addBox(buffers, cell + glm::vec3(s, 0, s), cell + glm::vec3(end, kCakeHeight, 1 - s),
         {tex.top, tex.side, tex.side, tex.side, cut, tex.side}, kCutoutNone, chunkWorldX,
         chunkWorldZ, skyLight, blockLight);
}

// A slab or a stair: its solid boxes (blockBoxes), in its material's tiles.
// A half-height side shows the whole tile, squeezed, as a bed's does.
void addShape(MeshBuffers &buffers, int x, int y, int z, BlockType type,
              int16_t chunkWorldX, int16_t chunkWorldZ, uint8_t skyLight, uint8_t blockLight) {
  const BlockTexture &tex = block_data.at(type);
  const BoxTiles tiles{tex.top, tex.bottom, tex.side, tex.side, tex.side, tex.side};
  const glm::vec3 cell(x, y, z);
  for (const DoorBox &box : blockBoxes(type))
    addBox(buffers, cell + box.lo, cell + box.hi, tiles, kCutoutNone, chunkWorldX, chunkWorldZ,
           skyLight, blockLight);
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

BlockType ChunkMesh::getBlock(World *world, const Chunk &chunk, int x, int y, int z,
                              BlockType unloaded) {
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
    return n ? n->at(local_x, y, local_z) : unloaded;
  }

  return chunk.at(x, y, z);
}

uint8_t ChunkMesh::getSkyLight(World *world, const Chunk &chunk, int x, int y, int z) {
  // Above the chunk height = full sky
  if (y >= kChunkHeight) return 15;
  if (y < 0) return 0;

  const int chunk_x = chunk.getPos()[0];
  const int chunk_z = chunk.getPos()[1];

  const int world_x = chunk_x * kChunkWidth + x;
  const int world_z = chunk_z * kChunkDepth + z;

  const int target_chunk_x = floor((float)world_x / kChunkWidth);
  const int target_chunk_z = floor((float)world_z / kChunkDepth);

  if (target_chunk_x != chunk_x || target_chunk_z != chunk_z) {
    auto n = world->getChunk(target_chunk_x, target_chunk_z);
    if (!n) return 15; // unloaded neighbor — assume full sky
    const int local_x = world_x - target_chunk_x * kChunkWidth;
    const int local_z = world_z - target_chunk_z * kChunkDepth;
    return n->safeSkyLight(local_x, y, local_z);
  }

  return chunk.safeSkyLight(x, y, z);
}

uint8_t ChunkMesh::getBlockLight(World *world, const Chunk &chunk, int x, int y, int z) {
  // Outside the vertical range there is no emitted light.
  if (y < 0 || y >= kChunkHeight) return 0;

  const int chunk_x = chunk.getPos()[0];
  const int chunk_z = chunk.getPos()[1];

  const int world_x = chunk_x * kChunkWidth + x;
  const int world_z = chunk_z * kChunkDepth + z;

  const int target_chunk_x = floor((float)world_x / kChunkWidth);
  const int target_chunk_z = floor((float)world_z / kChunkDepth);

  if (target_chunk_x != chunk_x || target_chunk_z != chunk_z) {
    auto n = world->getChunk(target_chunk_x, target_chunk_z);
    if (!n) return 0; // unloaded neighbor — no known light
    const int local_x = world_x - target_chunk_x * kChunkWidth;
    const int local_z = world_z - target_chunk_z * kChunkDepth;
    return n->safeBlockLight(local_x, y, local_z);
  }

  return chunk.safeBlockLight(x, y, z);
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

  // Side faces. Water shows one only toward open air (a cave pool's rim):
  // never toward land or more water, nor toward a chunk that hasn't loaded
  // (it would wall off the sea at the edge of view until it did).
  auto sideShows = [&](BlockType type, int nx, int ny, int nz) {
    if (!isLiquid(type)) return shouldRenderFace(type, getBlock(world, chunk, nx, ny, nz));
    return getBlock(world, chunk, nx, ny, nz, BlockType::WATER) == BlockType::AIR;
  };

  // Front face (+Z)
  for (int z = 0; z < kChunkDepth; ++z) {
    bool mask[kChunkWidth][kChunkHeight] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[x][y]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR || hasOwnShape(type))
          continue; // addDoor, addBed, addCrop

        if (sideShows(type, x, y, z + 1)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Front);
          uint8_t skyLight = getSkyLight(world, chunk, x, y, z + 1);
          uint8_t blockLight = getBlockLight(world, chunk, x, y, z + 1);

          int width = 1;
          while (mergesSides(type) && x + width < kChunkWidth && !mask[x + width][y] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 sideShows(type, x + width, y, z + 1) &&
                 getSkyLight(world, chunk, x + width, y, z + 1) == skyLight &&
                 getBlockLight(world, chunk, x + width, y, z + 1) == blockLight &&
                 computeFaceAO(world, chunk, x + width, y, z, Face::Front) == ao) {
            width++;
          }

          int height = 1;
          bool can_expand = mergesSides(type);
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[x + i][y + height] ||
                  chunk.safeAt(x + i, y + height, z) != type ||
                  !sideShows(type, x + i, y + height, z + 1) ||
                  getSkyLight(world, chunk, x + i, y + height, z + 1) != skyLight ||
                  getBlockLight(world, chunk, x + i, y + height, z + 1) != blockLight ||
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
                  isTransparent(type), cutoutClass(type), ao, skyLight, blockLight,
                  crystalHangs(world, chunk, type, x, y, z));
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
        if (type == BlockType::AIR || hasOwnShape(type))
          continue; // addDoor, addBed, addCrop

        if (sideShows(type, x, y, z - 1)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Back);
          uint8_t skyLight = getSkyLight(world, chunk, x, y, z - 1);
          uint8_t blockLight = getBlockLight(world, chunk, x, y, z - 1);

          int width = 1;
          while (mergesSides(type) && x + width < kChunkWidth && !mask[x + width][y] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 sideShows(type, x + width, y, z - 1) &&
                 getSkyLight(world, chunk, x + width, y, z - 1) == skyLight &&
                 getBlockLight(world, chunk, x + width, y, z - 1) == blockLight &&
                 computeFaceAO(world, chunk, x + width, y, z, Face::Back) == ao) {
            width++;
          }

          int height = 1;
          bool can_expand = mergesSides(type);
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[x + i][y + height] ||
                  chunk.safeAt(x + i, y + height, z) != type ||
                  !sideShows(type, x + i, y + height, z - 1) ||
                  getSkyLight(world, chunk, x + i, y + height, z - 1) != skyLight ||
                  getBlockLight(world, chunk, x + i, y + height, z - 1) != blockLight ||
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
                  isTransparent(type), cutoutClass(type), ao, skyLight, blockLight,
                  crystalHangs(world, chunk, type, x, y, z));
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
        if (type == BlockType::AIR || hasOwnShape(type))
          continue; // addDoor, addBed, addCrop

        BlockType neighbor = getBlock(world, chunk, x, y + 1, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Top);
          uint8_t skyLight = getSkyLight(world, chunk, x, y + 1, z);
          uint8_t blockLight = getBlockLight(world, chunk, x, y + 1, z);

          int width = 1;
          while (x + width < kChunkWidth && !mask[z][x + width] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y + 1, z)) &&
                 getSkyLight(world, chunk, x + width, y + 1, z) == skyLight &&
                 getBlockLight(world, chunk, x + width, y + 1, z) == blockLight &&
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
                  getSkyLight(world, chunk, x + i, y + 1, z + depth) != skyLight ||
                  getBlockLight(world, chunk, x + i, y + 1, z + depth) != blockLight ||
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
                  isTransparent(type), cutoutClass(type), ao, skyLight, blockLight);

          // For water at the surface (air above), also render bottom face
          // so it's visible from underwater looking up
          if (isLiquid(type) && neighbor == BlockType::AIR) {
            AO4 noAO = {{3, 3, 3, 3}};
            addQuad(buffers, x, y + 1, z, width, depth,
                    tex.bottom.x, tex.bottom.y, Face::Bottom, chunkWorldX, chunkWorldZ,
                    true, kCutoutNone, noAO, skyLight, blockLight);
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
        if (type == BlockType::AIR || hasOwnShape(type))
          continue; // addDoor, addBed, addCrop
        if (isLiquid(type)) continue;

        BlockType neighbor = getBlock(world, chunk, x, y - 1, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Bottom);
          uint8_t skyLight = getSkyLight(world, chunk, x, y - 1, z);
          uint8_t blockLight = getBlockLight(world, chunk, x, y - 1, z);

          int width = 1;
          while (x + width < kChunkWidth && !mask[z][x + width] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y - 1, z)) &&
                 getSkyLight(world, chunk, x + width, y - 1, z) == skyLight &&
                 getBlockLight(world, chunk, x + width, y - 1, z) == blockLight &&
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
                  getSkyLight(world, chunk, x + i, y - 1, z + depth) != skyLight ||
                  getBlockLight(world, chunk, x + i, y - 1, z + depth) != blockLight ||
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
                  isTransparent(type), cutoutClass(type), ao, skyLight, blockLight);
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
        if (type == BlockType::AIR || hasOwnShape(type))
          continue; // addDoor, addBed, addCrop

        if (sideShows(type, x + 1, y, z)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Right);
          uint8_t skyLight = getSkyLight(world, chunk, x + 1, y, z);
          uint8_t blockLight = getBlockLight(world, chunk, x + 1, y, z);

          int depth = 1;
          while (mergesSides(type) && z + depth < kChunkDepth && !mask[y][z + depth] &&
                 chunk.safeAt(x, y, z + depth) == type &&
                 sideShows(type, x + 1, y, z + depth) &&
                 getSkyLight(world, chunk, x + 1, y, z + depth) == skyLight &&
                 getBlockLight(world, chunk, x + 1, y, z + depth) == blockLight &&
                 computeFaceAO(world, chunk, x, y, z + depth, Face::Right) == ao) {
            depth++;
          }

          int height = 1;
          bool can_expand = mergesSides(type);
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < depth; ++i) {
              if (mask[y + height][z + i] ||
                  chunk.safeAt(x, y + height, z + i) != type ||
                  !sideShows(type, x + 1, y + height, z + i) ||
                  getSkyLight(world, chunk, x + 1, y + height, z + i) != skyLight ||
                  getBlockLight(world, chunk, x + 1, y + height, z + i) != blockLight ||
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
                  isTransparent(type), cutoutClass(type), ao, skyLight, blockLight,
                  crystalHangs(world, chunk, type, x, y, z));
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
        if (type == BlockType::AIR || hasOwnShape(type))
          continue; // addDoor, addBed, addCrop

        if (sideShows(type, x - 1, y, z)) {
          const auto &tex = block_data.at(type);
          AO4 ao = computeFaceAO(world, chunk, x, y, z, Face::Left);
          uint8_t skyLight = getSkyLight(world, chunk, x - 1, y, z);
          uint8_t blockLight = getBlockLight(world, chunk, x - 1, y, z);

          int depth = 1;
          while (mergesSides(type) && z + depth < kChunkDepth && !mask[y][z + depth] &&
                 chunk.safeAt(x, y, z + depth) == type &&
                 sideShows(type, x - 1, y, z + depth) &&
                 getSkyLight(world, chunk, x - 1, y, z + depth) == skyLight &&
                 getBlockLight(world, chunk, x - 1, y, z + depth) == blockLight &&
                 computeFaceAO(world, chunk, x, y, z + depth, Face::Left) == ao) {
            depth++;
          }

          int height = 1;
          bool can_expand = mergesSides(type);
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < depth; ++i) {
              if (mask[y + height][z + i] ||
                  chunk.safeAt(x, y + height, z + i) != type ||
                  !sideShows(type, x - 1, y + height, z + i) ||
                  getSkyLight(world, chunk, x - 1, y + height, z + i) != skyLight ||
                  getBlockLight(world, chunk, x - 1, y + height, z + i) != blockLight ||
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
                  isTransparent(type), cutoutClass(type), ao, skyLight, blockLight,
                  crystalHangs(world, chunk, type, x, y, z));
        }
      }
    }
  }

  // Doors, beds, crops, plants, torches, cactus and cake, which the passes
  // above leave out.
  for (int y = 0; y < kChunkHeight; ++y)
    for (int z = 0; z < kChunkDepth; ++z)
      for (int x = 0; x < kChunkWidth; ++x) {
        BlockType type = chunk.at(x, y, z);
        if (isDoor(type))
          addDoor(buffers, x, y, z, type, chunkWorldX, chunkWorldZ,
                  getSkyLight(world, chunk, x, y, z), getBlockLight(world, chunk, x, y, z));
        else if (isBed(type))
          addBed(buffers, x, y, z, type, chunkWorldX, chunkWorldZ,
                 getSkyLight(world, chunk, x, y, z), getBlockLight(world, chunk, x, y, z));
        else if (isCrop(type) || isPlant(type))
          addCrop(buffers, x, y, z, type, chunkWorldX, chunkWorldZ,
                  getSkyLight(world, chunk, x, y, z), getBlockLight(world, chunk, x, y, z));
        else if (isTorch(type))
          addTorch(buffers, x, y, z, type, chunkWorldX, chunkWorldZ,
                   getSkyLight(world, chunk, x, y, z), getBlockLight(world, chunk, x, y, z));
        else if (type == BlockType::CACTUS)
          addCactus(buffers, x, y, z, type, chunkWorldX, chunkWorldZ,
                    getSkyLight(world, chunk, x, y, z), getBlockLight(world, chunk, x, y, z));
        else if (isCake(type))
          addCake(buffers, x, y, z, type, chunkWorldX, chunkWorldZ,
                  getSkyLight(world, chunk, x, y, z), getBlockLight(world, chunk, x, y, z));
        else if (isHalfShape(type))
          addShape(buffers, x, y, z, type, chunkWorldX, chunkWorldZ,
                   getSkyLight(world, chunk, x, y, z), getBlockLight(world, chunk, x, y, z));
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
