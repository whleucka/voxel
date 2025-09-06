#pragma once

#include <glm/glm.hpp>
#include "mesh.hpp"
#include "block_type.hpp"

/**
 * block.hpp
 *
 * A single voxel block that owns a Mesh. It generates one cube worth of
 * vertices/indices using a texture atlas.
 *
 */
struct AtlasTile {
  int x; // tile column (0..tiles-1)
  int y; // tile row    (0..tiles-1)
};

struct BlockFaceTiles {
  AtlasTile px, nx, py, ny, pz, nz; // per-face tiles (+X, -X, +Y, -Y, +Z, -Z)
};

class Block {
public:
  Mesh mesh;
  BlockType type;

  /**
   * @param type        Block type (used to pick per-face tiles)
   * @param position    World position of the block's center
   * @param tilesPerAxis Number of tiles along one atlas dimension (e.g., 16 for 512/32)
   */
  Block(BlockType type,
        const glm::vec3& position = glm::vec3(0.0f),
        int tilesPerAxis = 16);

  // Regenerates the cube geometry/UVs based on type/atlas settings
  void generate();

  // Configure which tile each face should use for a given type.
  // Customize this mapping to your atlas layout.
  static BlockFaceTiles tilesFor(BlockType t);

  // Atlas grid size accessors
  int tilesPerAxis() const { return m_tilesPerAxis; }

private:
  glm::vec3 m_pos;
  int m_tilesPerAxis;

  // Helpers
  void emitFace(const glm::vec3 (&verts)[4],
                const glm::vec3& faceNormal,
                const AtlasTile& tile);
  void appendQuadIndices();

  // Converts an atlas tile (tx,ty) to the 4 UVs (bottom-left origin).
  // Returns UVs in order: (0) bl, (1) br, (2) tr, (3) tl
  void tileUVs(const AtlasTile& tile, glm::vec2 (&uv)[4]) const;
};
