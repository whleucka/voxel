#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;          // baseUV in block space
layout(location=3) in vec2 aTileOffset;  // atlas tile origin
layout(location=4) in vec2 aTileSpan;    // atlas tile span

uniform mat4 model, view, projection;

out vec3 vNormal;
out vec2 vBaseUV;
out vec2 vTileOffset;
out vec2 vTileSpan;

void main() {
    gl_Position = projection * view * model * vec4(aPosition, 1.0);
    vNormal     = aNormal;
    vBaseUV     = aUV;
    vTileOffset = aTileOffset;
    vTileSpan   = aTileSpan;
}
