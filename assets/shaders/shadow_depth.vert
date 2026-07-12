#version 330 core

// Must match block.vert vertex layout exactly
layout(location=0) in vec3 aPosPacked;
layout(location=1) in int aFaceId;
layout(location=2) in ivec2 aTileXY;
layout(location=3) in ivec2 aUV;
layout(location=4) in ivec2 aChunkOffset;
layout(location=5) in int aAO;

uniform mat4 uLightSpaceMatrix;

// Atlas constants — must match block.vert so the cutout test in the fragment
// shader samples exactly the same texels as the main pass.
const float TILE_SIZE = 32.0;
const float ATLAS_SIZE = 512.0;
const float TILE_STEP = TILE_SIZE / ATLAS_SIZE;
const float PADDING = 0.5 / ATLAS_SIZE;

out vec2 vBaseUV;
out vec2 vTileOffset;
out vec2 vTileSpan;
flat out float vCutoutThreshold;

// Must match block.vert exactly, or a leaf's shadow will not line up with the
// leaf that cast it.
const float CUTOUT_THRESHOLD[4] = float[4](0.0, 0.67, 0.80, 0.92);

void main() {
    // Decode position identically to block.vert
    vec3 localPos = aPosPacked * 0.5;
    vec3 worldPos = localPos + vec3(float(aChunkOffset.x), 0.0, float(aChunkOffset.y));

    gl_Position = uLightSpaceMatrix * vec4(worldPos, 1.0);

    vBaseUV     = vec2(aUV);
    vTileOffset = vec2(aTileXY) * TILE_STEP + vec2(PADDING);
    vTileSpan   = vec2(TILE_STEP - 2.0 * PADDING);

    // ao byte bits[7:6] = cutout class (see BlockVertex)
    vCutoutThreshold = CUTOUT_THRESHOLD[(aAO >> 6) & 3];
}
