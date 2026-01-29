#version 330 core
out vec4 FragColor;

in vec3 vNormal;
in vec2 vBaseUV;
in vec2 vTileOffset;
in vec2 vTileSpan;

uniform sampler2D uTexture;
uniform float uAlpha;

void main() {
    // Repeat every 1.0 unit in baseUV space (i.e., each block)
    vec2 tileUV  = fract(vBaseUV);              // 0..1 per block
    vec2 atlasUV = vTileOffset + tileUV * vTileSpan;
    vec4 texColor = texture(uTexture, atlasUV);
    FragColor = vec4(texColor.rgb, texColor.a * uAlpha);
}
