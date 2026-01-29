#pragma once

#include <cstdint>

// Packed vertex format: 16 bytes
// Position stored as int16 * 2 to handle 0.5 offsets (local to chunk)
// Chunk offset in world units for batch rendering
// Normal replaced with faceId (0-5) - reconstructed in shader
// Tile offset replaced with tile indices - computed in shader
struct BlockVertex {
  int16_t posX;        // 2 bytes - local_x * 2
  int16_t posY;        // 2 bytes - local_y * 2
  int16_t posZ;        // 2 bytes - local_z * 2
  uint8_t faceId;      // 1 byte  - 0=Top, 1=Bottom, 2=Left, 3=Right, 4=Front, 5=Back
  uint8_t tileX;       // 1 byte  - atlas tile X coordinate
  uint8_t tileY;       // 1 byte  - atlas tile Y coordinate
  uint8_t uvX;         // 1 byte  - baseUV.x (0-255)
  uint8_t uvY;         // 1 byte  - baseUV.y (0-255)
  uint8_t _pad;        // 1 byte  - padding
  int16_t chunkX;      // 2 bytes - chunk world offset X (chunk_x * CHUNK_WIDTH)
  int16_t chunkZ;      // 2 bytes - chunk world offset Z (chunk_z * CHUNK_DEPTH)
};

static_assert(sizeof(BlockVertex) == 16, "BlockVertex must be 16 bytes");
