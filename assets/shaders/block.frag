#version 330 core
out vec4 FragColor;

in vec3 outNormal;
in vec2 outUV;

uniform sampler2D uTexture;

void main() 
{
    vec4 texColor = texture(uTexture, outUV);

    // For now, ignore lighting and normals, just show the atlas color
    FragColor = texColor;
}
