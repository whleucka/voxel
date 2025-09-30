#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUvLocal;
layout (location = 3) in vec2 aUvBase;
layout (location = 4) in float aAO;

out float vAO;

uniform mat4 u_view;
uniform mat4 u_proj;
uniform float u_time;

void main() {
    vAO = aAO;
    vec3 world_pos = aPos;
    world_pos.x += u_time * 0.1;
    world_pos.z += u_time * 0.05;
    gl_Position = u_proj * u_view * vec4(world_pos, 1.0);
}

