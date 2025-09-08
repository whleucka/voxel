#include "block.hpp"
#include "vertex.hpp"

// Convenient alias
using V3 = glm::vec3;
using V2 = glm::vec2;

// ------ Face vertex templates for a unit cube centered at the origin ------
// We'll generate a 1x1x1 cube centered on m_pos. If you prefer 1 unit per block
// with min corner at (0,0,0), shift these accordingly.

// +X face (right)
static const V3 FACE_PX[4] = {V3(0.5f, -0.5f, -0.5f), V3(0.5f, -0.5f, 0.5f),
                              V3(0.5f, 0.5f, 0.5f), V3(0.5f, 0.5f, -0.5f)};
// -X face (left)
static const V3 FACE_NX[4] = {V3(-0.5f, -0.5f, 0.5f), V3(-0.5f, -0.5f, -0.5f),
                              V3(-0.5f, 0.5f, -0.5f), V3(-0.5f, 0.5f, 0.5f)};
// +Y face (top)
static const V3 FACE_PY[4] = {V3(-0.5f, 0.5f, -0.5f), V3(0.5f, 0.5f, -0.5f),
                              V3(0.5f, 0.5f, 0.5f), V3(-0.5f, 0.5f, 0.5f)};
// -Y face (bottom)
static const V3 FACE_NY[4] = {V3(-0.5f, -0.5f, 0.5f), V3(0.5f, -0.5f, 0.5f),
                              V3(0.5f, -0.5f, -0.5f), V3(-0.5f, -0.5f, -0.5f)};
// +Z face (front)
static const V3 FACE_PZ[4] = {V3(0.5f, -0.5f, 0.5f), V3(-0.5f, -0.5f, 0.5f),
                              V3(-0.5f, 0.5f, 0.5f), V3(0.5f, 0.5f, 0.5f)};
// -Z face (back)
static const V3 FACE_NZ[4] = {V3(-0.5f, -0.5f, -0.5f), V3(0.5f, -0.5f, -0.5f),
                              V3(0.5f, 0.5f, -0.5f), V3(-0.5f, 0.5f, -0.5f)};

Block::Block(BlockType t, const glm::vec3 &position, int tilesPerAxis)
    : type(t), m_pos(position), m_tilesPerAxis(tilesPerAxis) {
  generate();
}

void Block::generate() {
  mesh.vertices.clear();
  mesh.indices.clear();

  const auto map = tilesFor(type);

  // Emit six faces
  emitFace(FACE_PX, V3(1, 0, 0), map.px);
  emitFace(FACE_NX, V3(-1, 0, 0), map.nx);
  emitFace(FACE_PY, V3(0, 1, 0), map.py);
  emitFace(FACE_NY, V3(0, -1, 0), map.ny);
  emitFace(FACE_PZ, V3(0, 0, 1), map.pz);
  emitFace(FACE_NZ, V3(0, 0, -1), map.nz);

  // Upload to GPU (assumes Mesh has this method).
  mesh.setupMesh();
}

void Block::emitFace(const V3 (&verts)[4], const V3 &normal,
                     const AtlasTile &tile) {
  // Compute UVs for this face
  V2 uv[4];
  tileUVs(tile, uv);

  const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

  // Push 4 vertices (with position offset by m_pos)
  for (int i = 0; i < 4; ++i) {
    Vertex v;
    v.position = verts[i] + m_pos;
    v.normal = normal;
    v.texCoord = uv[i];
    mesh.vertices.push_back(v);
  }

  appendQuadIndices();
}

void Block::appendQuadIndices() {
  // Two triangles per quad: (0,1,2) (0,2,3) in local-quad space
  const uint32_t base = static_cast<uint32_t>(mesh.vertices.size() - 4);
  mesh.indices.push_back(base + 0);
  mesh.indices.push_back(base + 1);
  mesh.indices.push_back(base + 2);
  mesh.indices.push_back(base + 0);
  mesh.indices.push_back(base + 2);
  mesh.indices.push_back(base + 3);
}

void Block::tileUVs(const AtlasTile &tile, glm::vec2 (&uv)[4]) const {
  const float step = 1.0f / float(m_tilesPerAxis);

  // atlas is top-left origin (since STB flip is OFF), so flip V:
  const float u0 = tile.x * step;
  const float u1 = u0 + step;

  // flip V: bottom-left UV should sample from (1 - (y+1)*step) .. (1 - y*step)
  const float v0 = 1.0f - (tile.y * step + step);
  const float v1 = v0 + step;

  uv[0] = {u0, v0}; // bl
  uv[1] = {u1, v0}; // br
  uv[2] = {u1, v1}; // tr
  uv[3] = {u0, v1}; // tl
}

// Pick tiles per face for each block type.
// Adjust these indices to match your block_atlas.png layout.
BlockFaceTiles Block::tilesFor(BlockType t) {
  // Default all faces to the same tile (e.g., "stone" at 1,0)
  auto all = [](int x, int y) {
    BlockFaceTiles f{};
    f.px = f.nx = f.py = f.ny = f.pz = f.nz = AtlasTile{x, y};
    return f;
  };

  switch (t) {
  case BlockType::GRASS: {
    // Example: grass top (2,0), dirt bottom (0,1), grass side (1,0)
    BlockFaceTiles f{};
    f.py = AtlasTile{0, 0};             // +Y top
    f.px = f.nx = f.pz = f.nz = {1, 0}; // sides
    f.ny = AtlasTile{2, 0};             // -Y bottom (dirt)
    return f;
  }
  case BlockType::DIRT:
    return all(2, 0);
  case BlockType::STONE:
    return all(3, 0);
  case BlockType::BEDROCK:
    return all(4, 0);
  case BlockType::SAND:
    return all(5, 0);
  case BlockType::COBBLESTONE:
    return all(6, 0);
  default:
    return all(15, 15); // obvious "missing" tile in bottom-right
  }
}
