#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vTex;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    // transform normals into world space
    vNormal = mat3(transpose(inverse(model))) * aNormal;

    vTex = aTexCoord;
    gl_Position = projection * view * worldPos;
}
