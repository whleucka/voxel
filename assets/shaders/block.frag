#version 330 core
out vec4 FragColor;

in vec3 vNormal;
in vec2 vBaseUV;
in vec2 vTileOffset;
in vec2 vTileSpan;
in vec3 vWorldPos;

uniform sampler2D uTexture;
uniform float uAlpha;
uniform vec3 uCameraPos;
uniform bool uUnderwater;

// Directional lighting
uniform vec3 uSunDir;        // normalized sun direction (points toward sun)
uniform vec3 uSunColor;      // sun color * intensity
uniform vec3 uAmbientColor;  // ambient (sky + bounce) color

// Atmospheric fog (above water)
uniform vec3  uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

// Underwater fog
const vec3 WATER_FOG_COLOR = vec3(0.1, 0.3, 0.5);
uniform float uWaterFogDensity;

void main() {
    // ── Atlas sampling ────────────────────────────────────────────────────
    vec2 tileUV  = fract(vBaseUV);
    vec2 atlasUV = vTileOffset + tileUV * vTileSpan;
    vec4 texColor = texture(uTexture, atlasUV);

    // ── Face-based ambient occlusion (Minecraft-style depth cue) ─────────
    // Gives constant geometric depth even when the sun is on the horizon.
    float faceShade;
    if      (vNormal.y >  0.5) faceShade = 1.00;   // top    (+Y) – brightest
    else if (vNormal.y < -0.5) faceShade = 0.50;   // bottom (-Y) – darkest
    else if (abs(vNormal.x) > 0.5) faceShade = 0.70; // left / right
    else                            faceShade = 0.80; // front / back

    // ── Directional (sun/moon) diffuse ────────────────────────────────────
    float diffuse = max(dot(vNormal, uSunDir), 0.0);
    vec3  lighting = uAmbientColor + diffuse * uSunColor;

    vec3 finalColor = texColor.rgb * lighting * faceShade;

    // ── Fog ───────────────────────────────────────────────────────────────
    float dist = length(vWorldPos - uCameraPos);

    if (uUnderwater) {
        // Exponential underwater fog
        float fogFactor = 1.0 - exp(-uWaterFogDensity * dist);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        finalColor = mix(finalColor, WATER_FOG_COLOR, fogFactor);
    } else {
        // Linear atmospheric fog – starts at uFogStart, full at uFogEnd
        float fogFactor = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        finalColor = mix(finalColor, uFogColor, fogFactor);
    }

    FragColor = vec4(finalColor, texColor.a * uAlpha);
}
