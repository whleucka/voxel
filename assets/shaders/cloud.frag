#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;
in vec3 vNormal;

uniform float uTimeOfDay;    // 0..1
uniform vec3  uCameraPos;
uniform float uCloudFar;     // horizontal distance at which clouds fade out

const float PI = 3.14159265359;

void main() {
    // ── Day/night colouring ──────────────────────────────────────────────────
    float angle     = (uTimeOfDay - 0.25) * 2.0 * PI;
    float sinA      = sin(angle);
    float dayFactor = clamp(sinA + 0.1, 0.0, 1.0);
    float dawnFactor = pow(1.0 - abs(sinA), 4.0) * clamp(sinA + 0.15, 0.0, 1.0);

    vec3 dayColor   = vec3(1.0, 1.0, 1.0);
    vec3 nightColor = vec3(0.04, 0.05, 0.08);
    vec3 dawnColor  = vec3(1.0, 0.65, 0.35);

    vec3 cloudColor = mix(nightColor, dayColor, dayFactor);
    cloudColor = mix(cloudColor, dawnColor, dawnFactor * 0.5);

    // ── Minecraft-style face shading ─────────────────────────────────────────
    vec3 n = normalize(vNormal);
    float faceBrightness = 1.0;
    if (n.y >  0.5)          faceBrightness = 1.0;   // top — full sunlight
    else if (n.y < -0.5)     faceBrightness = 0.48;  // bottom — in shadow
    else if (abs(n.x) > 0.5) faceBrightness = 0.65;  // east/west sides
    else                     faceBrightness = 0.68;   // north/south sides

    cloudColor *= faceBrightness;

    // ── Distance fog: blend cloud colour toward sky colour at distance ───────
    // Compute sky colour to match renderer.cpp skyColor()
    vec3 kSkyDay   = vec3(0.53, 0.81, 0.92);
    vec3 kSkyNight = vec3(0.001, 0.001, 0.003);
    vec3 kSkyDawn  = vec3(0.85, 0.45, 0.20);
    vec3 fogColor  = mix(kSkyNight, kSkyDay, dayFactor);
    fogColor       = mix(fogColor, kSkyDawn, dawnFactor * 0.45);

    // Fades on horizontal distance, not 3D: the band is meant to read as a flat
    // ceiling, so a cloud directly overhead should stay solid no matter how high
    // the band is set.  uCloudFar scales with the band's altitude so the rim of
    // the disc stays down near the horizon, where this fade-to-sky colour is a
    // convincing match for the real sky.
    float dist      = length(vWorldPos.xz - uCameraPos.xz);
    float fogStart  = uCloudFar * 0.5;
    float fogFactor = clamp((dist - fogStart) / (uCloudFar - fogStart), 0.0, 1.0);

    // Fully faded — discard rather than draw a sky-coloured quad over the sky.
    if (fogFactor > 0.99) discard;

    cloudColor = mix(cloudColor, fogColor, fogFactor);

    FragColor = vec4(cloudColor, 1.0);
}
