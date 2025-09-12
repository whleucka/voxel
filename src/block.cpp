#include "block.hpp"
#include "vertex.hpp"

// Convenient alias
using V3 = glm::vec3;
using V2 = glm::vec2;

Block::Block(BlockType t, const glm::vec3 &position, int tilesPerAxis)
    : type(t), m_pos(position), m_tilesPerAxis(tilesPerAxis) {}

void Block::emitFace(const V3 (&verts)[4], const V3 &normal,
                     const AtlasTile &tile, Mesh &mesh) {
    V2 uv[4];
    tileUVs(tile, uv);

    // Base index before adding new vertices
    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

    for (int i = 0; i < 4; ++i) {
        Vertex v;
        v.position = verts[i] + m_pos;
        v.normal   = normal;
        v.texCoord = uv[i];
        mesh.vertices.push_back(v);
    }

    // Two triangles, CCW
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}

void Block::appendQuadIndices(Mesh &mesh) {
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

  const float u0 = tile.x * step;
  const float u1 = u0 + step;

  const float v0 = 1.0f - (tile.y * step + step);
  const float v1 = v0 + step;

  uv[0] = {u0, v0};
  uv[1] = {u1, v0};
  uv[2] = {u1, v1};
  uv[3] = {u0, v1};
}

BlockFaceTiles Block::tilesFor(BlockType t) {
  auto all = [](int x, int y) {
    BlockFaceTiles f{};
    f.px = f.nx = f.py = f.ny = f.pz = f.nz = AtlasTile{x, y};
    return f;
  };

  switch (t) {
  case BlockType::GRASS: {
    BlockFaceTiles f{};
    f.py = AtlasTile{0, 0};
    f.px = f.nx = f.pz = f.nz = {1, 0};
    f.ny = AtlasTile{2, 0};
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
    return all(15, 15);
  }
}
