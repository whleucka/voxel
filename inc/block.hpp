#pragma once

#include <glm/glm.hpp>
#include "mesh.hpp"
#include "block_type.hpp"

struct AtlasTile {
  int x;
  int y;
};

struct BlockFaceTiles {
  AtlasTile px, nx, py, ny, pz, nz;
};

/**
 * block.hpp
 *
 * Block class
 *
 */
class Block {
public:
  BlockType type;

  Block(BlockType type,
        const glm::vec3& position = glm::vec3(0.0f),
        int tilesPerAxis = 16);

  static BlockFaceTiles tilesFor(BlockType t);
  int tilesPerAxis() const { return tiles_per_axis; }

  void emitFace(const glm::vec3 (&verts)[4],
                const glm::vec3& faceNormal,
                const AtlasTile& tile, Mesh& mesh);
  void appendQuadIndices(Mesh& mesh);

private:
  glm::vec3 pos;
  int tiles_per_axis;

  void tileUVs(const AtlasTile& tile, glm::vec2 (&uv)[4]) const;
};
