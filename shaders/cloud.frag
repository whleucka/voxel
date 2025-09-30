#version 330 core

in float vAO;
out vec4 FragColor;

void main()
{
    // Base cloud color: light grayish white
    vec3 baseColor = vec3(0.9, 0.9, 0.9);

    FragColor = vec4(baseColor * vAO, 1.0);
}
