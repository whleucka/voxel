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
in float vBlockLight;
// Per-block alpha cutoff, looked up from the block's cutout class in block.vert.
// 0.0 for everything that is not a leaf.
flat in float vCutoutThreshold;

// Warm colour cast of block light sources (torches, glowstone, lava).
const vec3 BLOCK_LIGHT_COLOR = vec3(1.0, 0.80, 0.52);

uniform sampler2D uTexture;
uniform float uAlpha;
uniform vec3 uCameraPos;
uniform bool uUnderwater;
uniform float uTime;       // seconds, for the water's ripples

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

// Cloud shadows: which columns hold cloud, mapped over the cloud grid. The
// CPU twin is CloudShadowMap::shadeAt (cloud_shadow.hpp); keep them in step.
uniform sampler2D uCloudShadow;
uniform bool  uCloudShadows;
uniform vec2  uCloudOrigin;  // world x,z of the map's corner, drift included
uniform float uCloudSpan;    // the map's width in blocks
uniform float uCloudBase;    // world y of the cloud band's underside

// The share of direct sun that gets past the clouds to p (1 = all of it).
float cloudShade(vec3 p) {
    if (!uCloudShadows || uSunDir.y < 0.02) return 1.0;
    float t = (uCloudBase - p.y) / uSunDir.y;
    if (t < 0.0) return 1.0; // above the band
    vec2 at = p.xz + uSunDir.xz * t;
    float c = texture(uCloudShadow, (at - uCloudOrigin) / uCloudSpan).r;
    c = smoothstep(0.3, 0.7, c);
    return 1.0 - c * smoothstep(0.02, 0.2, uSunDir.y) * 0.85;
}

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

// ── Water ripples ────────────────────────────────────────────────────────
// A few sine waves running across the surface in different directions. The
// surface stays flat (the mesh doesn't move); only the way it takes the
// light ripples: its tilt shades it and throws back glints of the sun.
// Returns the slope of the surface (d height / d x, d height / d z) and, in
// `height`, how high the ripples stand here (-1..1).
vec2 rippleSlope(vec2 p, float t, out float height) {
    const vec2  DIR[4]   = vec2[4](vec2(0.80, 0.60), vec2(-0.55, 0.83),
                                   vec2(0.20, -0.98), vec2(-0.93, -0.37));
    const float FREQ[4]  = float[4](1.9, 2.7, 4.3, 6.1);
    const float SPEED[4] = float[4](1.3, 1.7, 2.3, 2.9);
    const float AMP[4]   = float[4](0.030, 0.022, 0.012, 0.008);
    vec2 slope = vec2(0.0);
    height = 0.0;
    for (int i = 0; i < 4; ++i) {
        float phase = dot(DIR[i], p) * FREQ[i] + t * SPEED[i];
        slope  += DIR[i] * FREQ[i] * AMP[i] * cos(phase);
        height += sin(phase) * AMP[i];
    }
    height /= 0.072; // the sum of AMP: -1..1
    return slope;
}

void main() {
    // Water is the only thing drawn in the blended pass (uAlpha < 1).
    bool  water    = uAlpha < 1.0;
    bool  surface  = water && vNormal.y > 0.5;
    vec3  N        = vNormal;
    float ripple   = 0.0;
    vec2  uvShift  = vec2(0.0);
    if (water) {
        vec2 slope = rippleSlope(vWorldPos.xz, uTime, ripple);
        if (surface) N = normalize(vec3(-slope.x, 1.0, -slope.y));
        // The tile drifts slowly and wobbles with the waves, so its pattern
        // flows instead of sitting still.
        uvShift = uTime * vec2(0.035, 0.022) + slope * 0.6;
    }

    // ── Atlas sampling ────────────────────────────────────────────────────
    vec2 tileUV  = fract(vBaseUV + uvShift);
    vec2 atlasUV = vTileOffset + tileUV * vTileSpan;
    vec4 texColor = texture(uTexture, atlasUV);

    // ── Alpha cutout ──────────────────────────────────────────────────────
    // Turns the soft edges of the leaf art into see-through holes.  Only leaves
    // carry a non-zero threshold: the test cannot be global, because the water
    // tile is a uniform alpha 0.6 and any cut that opens up the leaves would
    // erase every water surface in the world.
    if (texColor.a < vCutoutThreshold) discard;

    // ── Face-based ambient occlusion (Minecraft-style depth cue) ─────────
    // Gives constant geometric depth even when the sun is on the horizon.
    float faceShade;
    if      (vNormal.y >  0.5) faceShade = 1.00;   // top    (+Y) – brightest
    else if (vNormal.y < -0.5) faceShade = 0.50;   // bottom (-Y) – darkest
    else if (abs(vNormal.x) > 0.5) faceShade = 0.70; // left / right
    else                            faceShade = 0.80; // front / back

    // ── Directional (sun/moon) diffuse ────────────────────────────────────
    float diffuse = max(dot(N, uSunDir), 0.0);

    // Apply shadow: only the directional (sun) component is shadowed.
    // Ambient light is unaffected — shadowed areas still receive ambient.
    float shadow = calcShadow() * cloudShade(vWorldPos);
    vec3  lighting = uAmbientColor + shadow * diffuse * uSunColor;

    // Sky light: day/night colour (uAmbient + sun) × spatial sky visibility.
    // 0 = deep cave (5% brightness), 15 = full sky (100%).
    float skyLightFactor = mix(0.05, 1.0, vSkyLight);
    vec3  skyLit = lighting * skyLightFactor;

    // Block light: warm, unshadowed, and independent of time of day, so torches
    // glow at night and in caves. Smoothstep curve gives a natural falloff.
    float bl = vBlockLight * vBlockLight * (3.0 - 2.0 * vBlockLight);
    vec3  blockLit = BLOCK_LIGHT_COLOR * bl;

    // Take the brighter of the two per channel (avoids double-counting).
    vec3 lightLevel = max(skyLit, blockLit);

    vec3 finalColor = texColor.rgb * lightLevel * faceShade * vAO;

    // Ripples: crests a touch brighter than troughs, and the sun glinting
    // off the slopes that face it, only where sunlight reaches the water.
    float glint = 0.0;
    if (surface) {
        finalColor *= 1.0 + 0.08 * ripple;
        vec3 viewDir = normalize(uCameraPos - vWorldPos);
        vec3 halfway = normalize(uSunDir + viewDir);
        glint = pow(max(dot(N, halfway), 0.0), 90.0) * shadow * vSkyLight *
                step(0.0, uSunDir.y);
        finalColor += uSunColor * glint * 0.6;
    }

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
    // Applies to water's top and side faces (water viewed from the air), so
    // the wall of a hole dug in the sea reads as water like the surface does,
    // not as a faint grey film. The underside, seen from below, keeps its own.
    float finalAlpha = texColor.a * uAlpha;
    if (uAlpha < 1.0 && vNormal.y > -0.5) {
        // Fresnel: looking straight down → see-through; grazing angle → opaque
        vec3  viewDir = normalize(uCameraPos - vWorldPos);
        float NdotV   = clamp(dot(N, viewDir), 0.0, 1.0);
        float fresnel = pow(1.0 - NdotV, 2.5);
        finalAlpha = clamp(mix(uAlpha, 0.96, fresnel), 0.0, 1.0);
        // A side face is the wall of a hole: behind it is the whole sea, not
        // a thin film, so it is near opaque even seen straight on.
        if (vNormal.y < 0.5) finalAlpha = max(finalAlpha, 0.92);

        // Subtle blue-green tint – quadratic ambient scaling so it
        // nearly vanishes at night (linear was still ~6-16% teal after sRGB gamma).
        float brightness = clamp(length(uAmbientColor) * 3.5, 0.0, 1.0);
        brightness = brightness * brightness;           // quadratic drop-off
        // ...and only as bright as the light where the water is: daylight
        // reaches a cave pool only as far as the sky does, and a crystal or
        // torch tints it near the light, so dark cave water stays dark.
        brightness = max(brightness * vSkyLight, bl);
        vec3  waterTint  = vec3(0.04, 0.26, 0.38) * brightness;
        finalColor = mix(finalColor, waterTint, 0.28 * brightness);
        finalAlpha = max(finalAlpha, glint); // a glint isn't see-through
    }

    FragColor = vec4(finalColor, finalAlpha);
}
