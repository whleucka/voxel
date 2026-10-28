#pragma once

#include <cstdint>

// Must match the 1/16 in block.vert and shadow_depth.vert.
constexpr float kPosScale = 16.0f;

// Packed vertex format: 16 bytes
// Position stored as int16 * kPosScale, to 1/16 of a block (local to chunk),
// fine enough for a door's thin panel
// Chunk offset in world units for batch rendering
// Normal replaced with faceId (0-5) - reconstructed in shader
// Tile offset replaced with tile indices - computed in shader
struct BlockVertex {
  int16_t posX;        // 2 bytes - local_x * kPosScale
  int16_t posY;        // 2 bytes - local_y * kPosScale
  int16_t posZ;        // 2 bytes - local_z * kPosScale
  uint8_t faceId;      // 1 byte  - 0=Top, 1=Bottom, 2=Left, 3=Right, 4=Front, 5=Back
  uint8_t tileX;       // 1 byte  - atlas tile X coordinate
  uint8_t tileY;       // 1 byte  - atlas tile Y coordinate
  uint8_t uvX;         // 1 byte  - baseUV.x (0-255)
  uint8_t uvY;         // 1 byte  - baseUV.y (0-255)
  uint8_t ao;          // 1 byte  - packed: bits[1:0]=AO level (0-3), bits[5:2]=sky light (0-15), bits[7:6]=cutout class (0-3)
  int16_t chunkX;      // 2 bytes - chunk world offset X (chunk_x * CHUNK_WIDTH)
  int16_t chunkZ;      // 2 bytes - chunk world offset Z (chunk_z * CHUNK_DEPTH)
};

static_assert(sizeof(BlockVertex) == 16, "BlockVertex must be 16 bytes");
