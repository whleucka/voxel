#version 330 core
out vec4 FragColor;

in vec3 vNormal;
in vec2 vBaseUV;
in vec2 vTileOffset;
in vec2 vTileSpan;
in vec3 vWorldPos;
in vec4 vLightSpacePos;
in float vAO;
in float vSkyLight;

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

// Shadow mapping
uniform sampler2DShadow uShadowMap;
uniform bool uShadowsEnabled;

// ── Shadow calculation with PCF ─────────────────────────────────────────
uniform mat4 uLightSpaceMatrix;

float calcShadow() {
    if (!uShadowsEnabled) return 1.0;

    // Normal offset: push the sample point away from the surface along
    // its normal before projecting into light space.  This eliminates
    // self-shadowing artifacts (shimmering slivers at block bases) by
    // ensuring the lookup point is clearly on the lit side of the geometry.
    float NdotL = dot(vNormal, uSunDir);
    float normalOffsetScale = clamp(1.0 - NdotL, 0.0, 1.0) * 0.4 + 0.05;
    vec3 offsetPos = vWorldPos + vNormal * normalOffsetScale;

    // Project the offset position into light space
    vec4 lsPos = uLightSpaceMatrix * vec4(offsetPos, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    // Transform from [-1,1] to [0,1] for texture lookup
    projCoords = projCoords * 0.5 + 0.5;

    // Fragments outside the shadow map are fully lit
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0)
        return 1.0;

    // Small residual bias on top of the polygon offset + normal offset
    float bias = max(0.001 * (1.0 - NdotL), 0.0002);
    float currentDepth = projCoords.z - bias;

    // 3x3 PCF (percentage-closer filtering) for soft shadow edges
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec3 sampleCoord = vec3(projCoords.xy + vec2(x, y) * texelSize, currentDepth);
            shadow += texture(uShadowMap, sampleCoord);
        }
    }
    shadow /= 9.0;

    return shadow;
}

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

    // Apply shadow: only the directional (sun) component is shadowed.
    // Ambient light is unaffected — shadowed areas still receive ambient.
    float shadow = calcShadow();
    vec3  lighting = uAmbientColor + shadow * diffuse * uSunColor;

    // Sky-light attenuation: 0 = deep cave (5% brightness), 15 = full sky (100%)
    float skyLightFactor = mix(0.05, 1.0, vSkyLight);

    vec3 finalColor = texColor.rgb * lighting * faceShade * vAO * skyLightFactor;

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
