#version 330 core

// Packed vertex attributes
layout(location=0) in vec3 aPosPacked;  // int16 * 2, decoded as short
layout(location=1) in int aFaceId;      // 0-5: Top, Bottom, Left, Right, Front, Back
layout(location=2) in ivec2 aTileXY;    // Atlas tile coordinates
layout(location=3) in ivec2 aUV;        // Base UV (0-255)
layout(location=4) in ivec2 aChunkOffset; // Chunk world offset (X, Z)

uniform mat4 view, projection;

// Atlas constants
const float TILE_SIZE = 32.0;
const float ATLAS_SIZE = 512.0;
const float TILE_STEP = TILE_SIZE / ATLAS_SIZE;  // 0.0625
const float PADDING = 0.5 / ATLAS_SIZE;          // Half texel padding

// Normal lookup table (indexed by faceId)
const vec3 NORMALS[6] = vec3[6](
    vec3( 0.0,  1.0,  0.0),  // 0: Top    (+Y)
    vec3( 0.0, -1.0,  0.0),  // 1: Bottom (-Y)
    vec3(-1.0,  0.0,  0.0),  // 2: Left   (-X)
    vec3( 1.0,  0.0,  0.0),  // 3: Right  (+X)
    vec3( 0.0,  0.0,  1.0),  // 4: Front  (+Z)
    vec3( 0.0,  0.0, -1.0)   // 5: Back   (-Z)
);

out vec3 vNormal;
out vec2 vBaseUV;
out vec2 vTileOffset;
out vec2 vTileSpan;

void main() {
    // Decode local position (was multiplied by 2 to preserve 0.5 precision)
    vec3 localPos = aPosPacked * 0.5;

    // Add chunk offset to get world position
    vec3 worldPos = localPos + vec3(float(aChunkOffset.x), 0.0, float(aChunkOffset.y));

    gl_Position = projection * view * vec4(worldPos, 1.0);

    // Lookup normal from face ID
    vNormal = NORMALS[aFaceId];

    // Pass base UV as float (for texture repeating)
    vBaseUV = vec2(aUV);

    // Compute tile offset in atlas with padding
    vTileOffset = vec2(aTileXY) * TILE_STEP + vec2(PADDING);

    // Tile span with padding removed from both sides
    vTileSpan = vec2(TILE_STEP - 2.0 * PADDING);
}
