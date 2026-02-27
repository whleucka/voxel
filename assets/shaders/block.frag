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
uniform vec3  uWaterFogColor;   // time-of-day adjusted in renderer
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
        finalColor = mix(finalColor, uWaterFogColor, fogFactor);
    } else {
        // Linear atmospheric fog – starts at uFogStart, full at uFogEnd
        float fogFactor = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        finalColor = mix(finalColor, uFogColor, fogFactor);
    }

    // ── Water surface: Fresnel opacity + blue-green tint ──────────────────
    // Applies only to transparent top faces (i.e. water viewed from above).
    float finalAlpha = texColor.a * uAlpha;
    if (uAlpha < 1.0 && vNormal.y > 0.5) {
        // Fresnel: looking straight down → see-through; grazing angle → opaque
        vec3  viewDir = normalize(uCameraPos - vWorldPos);
        float NdotV   = clamp(dot(vNormal, viewDir), 0.0, 1.0);
        float fresnel = pow(1.0 - NdotV, 2.5);
        finalAlpha = clamp(mix(uAlpha, 0.96, fresnel), 0.0, 1.0);

        // Subtle blue-green tint – quadratic ambient scaling so it
        // nearly vanishes at night (linear was still ~6-16% teal after sRGB gamma).
        float brightness = clamp(length(uAmbientColor) * 3.5, 0.0, 1.0);
        brightness = brightness * brightness;           // quadratic drop-off
        vec3  waterTint  = vec3(0.04, 0.26, 0.38) * brightness;
        finalColor = mix(finalColor, waterTint, 0.28 * brightness);
    }

    FragColor = vec4(finalColor, finalAlpha);
}
