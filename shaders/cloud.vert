#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 u_view;
uniform mat4 u_proj;
uniform float u_time;

void main() {
    vec3 world_pos = aPos;
    world_pos.x += u_time * 2.0; // Wind speed
    gl_Position = u_proj * u_view * vec4(world_pos, 1.0);
}
