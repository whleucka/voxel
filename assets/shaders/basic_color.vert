#version 330 core
layout(location = 0) in vec3 aPos; // position
layout(location = 1) in vec3 aColor; // colour

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 outColor;

void main() 
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    outColor = aColor;
}
