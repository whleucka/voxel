#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUvLocal;
layout(location=3) in vec2 aUvBase;
layout(location=4) in float aAO;

uniform mat4 model, view, projection;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUvLocal;
out vec2 vUvBase;
out float vAO;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;

    // model is pure translate for chunks, but this is still fine:
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vWorldPos = worldPos.xyz;

    vUvLocal = aUvLocal; // in blocks (0..w, 0..h)
    vUvBase  = aUvBase;  // lower-left corner of atlas tile [0..1]
    vAO = aAO;
}
