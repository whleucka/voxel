#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUvLocal; // in blocks
in vec2 vUvBase;  // tile base (u0,v0) in [0..1]
in float vAO;

out vec4 FragColor;

uniform sampler2D texture_diffuse1;

// lighting / fog uniforms
uniform vec3 lightDir;
uniform vec3 ambientColor;
uniform float uSunVis;
uniform float sunStrength;
uniform vec3 cameraPos;
uniform vec3 fogColor;
uniform vec3 sunColor;
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
    // TEXTURE
    // Tile every block by taking fractional part
    vec2 st01 = fract(vUvLocal);
    // Map inside the tile with a small border to avoid bleeding
    vec2 uv = vUvBase + PAD2 + st01 * (TILE_ST - 2.0 * PAD2);

    vec4 texColor = texture(texture_diffuse1, uv);

    // LIGHTING
    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightDir);

    // wrap diffuse or plain lambert — but multiply by uSunVis
    float wrap = 0.3;
    float ndl  = dot(N, L);
    float diff = clamp((ndl + wrap) / (1.0 + wrap), 0.0, 1.0);
    diff *= uSunVis; // <-- no direct sun when below horizon

    // NIGHT AMBIENT: lerp down ambient at night (e.g., 0.05 at night, 0.25 day)
    float nightAmb = 0.1;
    float dayAmb   = 0.25;
    float ambScale = mix(nightAmb, dayAmb, uSunVis);

    // If you use hemisphere ambient, multiply that result by ambScale instead.
    vec3 lit = ambientColor * ambScale * vAO + diff * sunStrength;
    vec3 litColor = texColor.rgb * lit;  // (linear space)

    // Fog
    float density = 0.005;
    float dist = length(vWorldPos - cameraPos);
    float fogFactor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);

    vec3 finalColor = mix(fogColor, litColor, fogFactor);
    FragColor = vec4(finalColor, texColor.a);
}
