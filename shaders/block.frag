#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vTex;

out vec4 FragColor;

uniform sampler2D texture_diffuse1;

// Sun directional light
uniform vec3 lightDir;
uniform vec3 ambientColor;
uniform float sunStrength;

// Fog
uniform vec3 cameraPos;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

void main()
{
    vec4 texColor = texture(texture_diffuse1, vTex);

    // --- Lighting ---
    float diff = max(dot(normalize(vNormal), normalize(lightDir)), 0.0);
    vec3 lighting = ambientColor + diff * sunStrength;
    vec3 litColor = texColor.rgb * lighting;

    // --- Fog (exponential squared) ---
    float density = 0.005; // (smaller = farther fog)
    float dist = length(vWorldPos - cameraPos);
    float fogFactor = exp(-pow(dist * density, 2.0)); 

    // clamp to [0,1]
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // Blend: when fogFactor=1.0 → no fog, when fogFactor=0.0 → full fog
    vec3 finalColor = mix(fogColor, litColor, fogFactor);

    FragColor = vec4(finalColor, texColor.a);
}