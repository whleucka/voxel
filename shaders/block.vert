#version 330 core

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTex;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vTex;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal   = mat3(model) * aNormal;
    vTex      = aTex;
    gl_Position = projection * view * worldPos;
}
