#version 330 core
// One box of an entity model: a unit cube scaled and placed by uModel.
layout (location = 0) in vec3 aPos;     // 0..1
layout (location = 1) in vec3 aNormal;  // cube-local face normal
layout (location = 2) in vec2 aUV;      // 0..1 across the face, v = 0 at its top

uniform mat4 uModel;
uniform mat3 uNormalMat;
uniform mat4 view;
uniform mat4 projection;

out vec3 vNormal;
out vec2 vUV;
out vec3 vWorldPos;
flat out vec3 vLocalNormal;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = normalize(uNormalMat * aNormal);
    vLocalNormal = aNormal;
    vUV = aUV;
    gl_Position = projection * view * world;
}
