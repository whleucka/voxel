#version 330 core
out vec4 FragColor;

in vec3 vDir;

uniform float uTimeOfDay; // 0..1  (0 = midnight, 0.25 = dawn, 0.5 = noon, 0.75 = dusk)

const float PI = 3.14159265359;

// ── Colour palette ────────────────────────────────────────────────────────────
const vec3 kZenithDay    = vec3(0.08, 0.35, 0.78);
const vec3 kHorizonDay   = vec3(0.53, 0.81, 0.92);
const vec3 kZenithNight  = vec3(0.000, 0.000, 0.002);
const vec3 kHorizonNight = vec3(0.001, 0.001, 0.003);
const vec3 kDawnColor    = vec3(0.90, 0.46, 0.14);
const vec3 kSunColor     = vec3(1.00, 0.96, 0.72);
const vec3 kMoonColor    = vec3(0.78, 0.84, 1.00);

// ── Simple hash for star field ────────────────────────────────────────────────
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec3 dir = normalize(vDir);

    // ── Sun / moon direction (matches renderer.cpp) ───────────────────────────
    float angle   = (uTimeOfDay - 0.25) * 2.0 * PI;
    float sinA    = sin(angle);
    float cosA    = cos(angle);
    vec3  sunDir  = normalize(vec3(cosA * 0.6, sinA, 0.3));
    vec3  moonDir = -sunDir;

    float dayFactor   = clamp(sinA + 0.1,  0.0, 1.0);
    float nightFactor = 1.0 - dayFactor;
    float dawnFactor  = pow(1.0 - abs(sinA), 4.0) * clamp(sinA + 0.15, 0.0, 1.0);

    // ── Sky gradient (zenith → horizon) ──────────────────────────────────────
    float elev      = clamp(dir.y, 0.0, 1.0);
    float horizBlend = pow(1.0 - elev, 4.0);

    vec3 zenith  = mix(kZenithNight,  kZenithDay,  dayFactor);
    vec3 horizon = mix(kHorizonNight, kHorizonDay, dayFactor);
    horizon = mix(horizon, kDawnColor, dawnFactor * 0.65);

    vec3 sky = mix(zenith, horizon, horizBlend);

    // ── Dawn/dusk directional glow near the horizon ───────────────────────────
    vec3  sunAz2d   = normalize(vec3(sunDir.x, 0.0, sunDir.z));
    vec3  dirAz2d   = normalize(vec3(dir.x, 0.0, dir.z));
    float sunAzDot  = max(dot(dirAz2d, sunAz2d), 0.0);
    float horizGlow = pow(sunAzDot, 6.0) * dawnFactor * (1.0 - elev);
    sky = mix(sky, kDawnColor * 1.35, horizGlow * 0.45);

    // ── Sun soft glow halo (kept for atmosphere) ──────────────────────────────
    float sunDot  = dot(dir, sunDir);
    float sunGlow = pow(max(sunDot, 0.0), 96.0) * dayFactor;
    sky += kSunColor * sunGlow * 0.55;

    // ── Block-style sun (pixelated square) ────────────────────────────────────
    {
        // Build a stable tangent frame around sunDir.
        // sunDir always has a non-zero XZ component so cross with world-up is safe.
        vec3 sRight = normalize(cross(vec3(0.0, 1.0, 0.0), sunDir));
        vec3 sUp    = normalize(cross(sunDir, sRight));

        float lx = dot(dir, sRight);
        float ly = dot(dir, sUp);

        const float kHalf  = 0.070;          // angular half-size of the square
        const float kPixel = kHalf / 3.5;    // 7×7 pixel grid

        // Snap to pixel centres
        float px = (floor(lx / kPixel) + 0.5) * kPixel;
        float py = (floor(ly / kPixel) + 0.5) * kPixel;

        if (abs(px) < kHalf && abs(py) < kHalf) {
            // Slightly brighter core (inner 3×3 pixels)
            float core = (abs(px) < kHalf * 0.45 && abs(py) < kHalf * 0.45) ? 1.4 : 1.0;
            sky = mix(sky, kSunColor * 2.6 * core, dayFactor);
        }
    }

    // ── Block-style moon (pixelated square) ───────────────────────────────────
    {
        vec3 mRight = normalize(cross(vec3(0.0, 1.0, 0.0), moonDir));
        vec3 mUp    = normalize(cross(moonDir, mRight));

        float lx = dot(dir, mRight);
        float ly = dot(dir, mUp);

        const float kHalf  = 0.052;
        const float kPixel = kHalf / 3.0;    // 6×6 pixel grid

        float px = (floor(lx / kPixel) + 0.5) * kPixel;
        float py = (floor(ly / kPixel) + 0.5) * kPixel;

        if (abs(px) < kHalf && abs(py) < kHalf) {
            sky = mix(sky, kMoonColor * 1.8, nightFactor);
        }
    }

    // ── Stars ─────────────────────────────────────────────────────────────────
    float starFade = clamp(dir.y * 4.0 + 0.2, 0.0, 1.0) * nightFactor;
    if (starFade > 0.001) {
        // Longitude / latitude → UV
        float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
        float v = asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5;

        vec2 starUV  = vec2(u, v) * 200.0;
        vec2 starId  = floor(starUV);
        vec2 starFrac = fract(starUV);

        float h = hash(starId);
        if (h > 0.965) { // ~3.5 % of cells contain a star
            vec2  center = vec2(hash(starId + vec2(17.0, 0.0)),
                                hash(starId + vec2(0.0, 17.0)));
            float dist    = length(starFrac - center);
            float star    = smoothstep(0.09, 0.0, dist);
            // Subtle twinkle based on star's hash phase
            float twinkle = 0.65 + 0.35 * sin(h * 314.159 + uTimeOfDay * 4000.0);
            sky += star * twinkle * starFade * 0.85;
        }
    }

    // Below-horizon: the gradient already resolves to the horizon colour here
    // (elev is clamped to 0 → horizBlend = 1 → sky = horizon).
    // Intentionally NO separate fill: any brownish fill colour would mismatch
    // the fog colour applied to terrain at the same angle, creating a visible
    // seam at the chunk boundary.

    FragColor = vec4(sky, 1.0);
}
