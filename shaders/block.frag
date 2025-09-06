#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vTex;

out vec4 FragColor;

// Mesh::draw binds this as texture unit 0 and sets the uniform name to "texture_diffuse1"
uniform sampler2D texture_diffuse1;

void main()
{
    vec4 texColor = texture(texture_diffuse1, vTex);

    // simple one-light lambert (with small ambient so dark faces aren't pitch black)
    const vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float lambert = max(dot(normalize(vNormal), lightDir), 0.2);

    FragColor = vec4(texColor.rgb * lambert, texColor.a);
}
