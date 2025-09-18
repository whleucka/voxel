#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUvLocal; // in blocks
in vec2 vUvBase;  // tile base (u0,v0) in [0..1]

out vec4 FragColor;

uniform sampler2D texture_diffuse1;

// lighting / fog uniforms
uniform vec3 lightDir;
uniform vec3 ambientColor;
uniform float sunStrength;
uniform vec3 cameraPos;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

// atlas constants
const float ATLAS_PX = 256.0;
const float TILE_PX  = 16.0;
const vec2  TILE_ST  = vec2(TILE_PX / ATLAS_PX);
const float PAD      = 0.5 / ATLAS_PX;           // half texel
const vec2  PAD2     = vec2(PAD);

void main()
{
    // Tile every block by taking fractional part
    vec2 st01 = fract(vUvLocal);
    // Map inside the tile with a small border to avoid bleeding
    vec2 uv = vUvBase + PAD2 + st01 * (TILE_ST - 2.0 * PAD2);

    vec4 texColor = texture(texture_diffuse1, uv);

    // Simple lambert
    float diff = max(dot(normalize(vNormal), normalize(lightDir)), 0.0);
    vec3 lighting = ambientColor + diff * sunStrength;
    vec3 litColor = texColor.rgb * lighting;

    // Fog
    float density = 0.005;
    float dist = length(vWorldPos - cameraPos);
    float fogFactor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);

    vec3 finalColor = mix(fogColor, litColor, fogFactor);
    FragColor = vec4(finalColor, texColor.a);
}
