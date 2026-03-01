#version 330 core

// Must match block.vert vertex layout exactly
layout(location=0) in vec3 aPosPacked;
layout(location=1) in int aFaceId;
layout(location=2) in ivec2 aTileXY;
layout(location=3) in ivec2 aUV;
layout(location=4) in ivec2 aChunkOffset;
layout(location=5) in int aAO;

uniform mat4 uLightSpaceMatrix;

void main() {
    // Decode position identically to block.vert
    vec3 localPos = aPosPacked * 0.5;
    vec3 worldPos = localPos + vec3(float(aChunkOffset.x), 0.0, float(aChunkOffset.y));

    gl_Position = uLightSpaceMatrix * vec4(worldPos, 1.0);
}
