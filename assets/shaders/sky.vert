#version 330 core
layout(location = 0) in vec3 aPos;

out vec3 vDir;

uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vDir = aPos;

    // Strip translation from view matrix – only rotation matters for the sky
    mat4 rotView = mat4(mat3(uView));

    // Set z = w so the sky fragment always has NDC depth = 1.0 (max depth),
    // which means any real world geometry with z < 1.0 will overwrite it.
    vec4 pos = uProjection * rotView * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
