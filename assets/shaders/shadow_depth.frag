#version 330 core

// Depth-only pass: the GPU writes depth automatically, so there is no colour
// output.  The atlas is still sampled to honour the alpha cutout — a leaf's
// holes must not occlude, otherwise the canopy casts a solid slab of shadow
// instead of a dappled one.

in vec2 vBaseUV;
in vec2 vTileOffset;
in vec2 vTileSpan;
flat in float vCutoutThreshold;  // 0.0 for everything that is not a leaf

uniform sampler2D uTexture;

void main() {
    if (vCutoutThreshold > 0.0) {
        vec2 tileUV  = fract(vBaseUV);
        vec2 atlasUV = vTileOffset + tileUV * vTileSpan;
        if (texture(uTexture, atlasUV).a < vCutoutThreshold) discard;
    }
    // gl_FragDepth is written automatically
}
