#version 330 core
layout(location = 0) in vec3 aPosition; // position
layout(location = 1) in vec3 aNormal;   // normal
layout(location = 2) in vec2 aUV;       // texture uv

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 outNormal;
out vec2 outUV;

void main() 
{
    gl_Position = projection * view * model * vec4(aPosition, 1.0);
    outNormal = aNormal;
    outUV = aUV;
}
