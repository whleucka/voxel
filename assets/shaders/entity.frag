#version 330 core
// Entity boxes, lit like blocks (see block.frag) but with one light sample
// per entity instead of per vertex.
out vec4 FragColor;

in vec3 vNormal;
in vec2 vUV;
in vec3 vWorldPos;
flat in vec3 vLocalNormal;
in vec3 vLocalPos;

const vec3 BLOCK_LIGHT_COLOR = vec3(1.0, 0.80, 0.52);

uniform int   uMode;        // 0 = flat colour, 1 = block faces, 2 = item card
uniform vec3  uColor;       // sRGB, for uMode 0
uniform sampler2D uAtlas;
uniform vec2  uTileTop;     // atlas UV of each face's tile (top-left)
uniform vec2  uTileSide;
uniform vec2  uTileBottom;
uniform float uTileSpan;    // atlas UV size of one tile
uniform vec3  uCellLo;      // uMode 1: where this box sits in its block's cell
uniform vec3  uCellHi;      // (0..1), so each face shows its part of the tile

uniform float uSkyLight;    // 0..1 at the entity
uniform float uBlockLight;  // 0..1 at the entity
uniform float uHurt;        // 0..1 red tint
uniform float uBurn;        // 0..1 orange glow (set alight)
uniform bool  uGlow;        // shine at full brightness, ignoring the light

uniform vec3  uSunDir;
uniform float uCloudShade;  // share of direct sun past the clouds, at the entity
uniform vec3  uSunColor;
uniform vec3  uAmbientColor;
uniform vec3  uCameraPos;
uniform bool  uUnderwater;
uniform vec3  uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec3  uWaterFogColor;
uniform float uWaterFogDensity;

void main() {
    vec3 base;
    if (uMode == 0) {
        base = pow(uColor, vec3(2.2)); // sRGB in, linear for lighting
    } else {
        // Item cards only show their front and back.
        if (uMode == 2 && abs(vLocalNormal.z) < 0.5) discard;
        vec2 tile = vLocalNormal.y > 0.5  ? uTileTop
                  : vLocalNormal.y < -0.5 ? uTileBottom
                                          : uTileSide;
        vec2 uv = vUV;
        if (uMode == 1) {
            // The face's part of the tile, as a block's would be: the same
            // mapping as the unit cube's UVs, but across the whole cell.
            vec3 p = mix(uCellLo, uCellHi, vLocalPos);
            vec3 n = vLocalNormal;
            uv = n.x >  0.5 ? vec2(1.0 - p.z, 1.0 - p.y)
               : n.x < -0.5 ? vec2(p.z, 1.0 - p.y)
               : n.z >  0.5 ? vec2(p.x, 1.0 - p.y)
               : n.z < -0.5 ? vec2(1.0 - p.x, 1.0 - p.y)
               : n.y >  0.5 ? vec2(p.x, p.z)
                            : vec2(p.x, 1.0 - p.z);
        }
        vec4 tex = texture(uAtlas, tile + clamp(uv, 0.001, 0.999) * uTileSpan);
        if (tex.a < 0.5) discard;
        base = tex.rgb;
    }

    float faceShade;
    if      (vNormal.y >  0.5)     faceShade = 1.00;
    else if (vNormal.y < -0.5)     faceShade = 0.50;
    else if (abs(vNormal.x) > 0.5) faceShade = 0.70;
    else                           faceShade = 0.80;

    float diffuse = max(dot(vNormal, uSunDir), 0.0);
    vec3  lighting = uAmbientColor + diffuse * uCloudShade * uSunColor;
    vec3  skyLit = lighting * mix(0.05, 1.0, uSkyLight);
    float bl = uBlockLight * uBlockLight * (3.0 - 2.0 * uBlockLight);
    vec3  lightLevel = max(skyLit, BLOCK_LIGHT_COLOR * bl);

    vec3 color = base * lightLevel * faceShade;
    color = mix(color, vec3(0.75, 0.04, 0.04) * max(lightLevel, vec3(0.25)), uHurt);
    color = mix(color, vec3(1.0, 0.55, 0.12), uBurn); // fire lights itself
    if (uGlow) color = base; // glowing eyes: unlit, so they read in the dark

    float dist = length(vWorldPos - uCameraPos);
    if (uUnderwater) {
        float f = clamp(1.0 - exp(-uWaterFogDensity * dist), 0.0, 1.0);
        color = mix(color, uWaterFogColor, f);
    } else {
        float f = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
        color = mix(color, uFogColor, f);
    }
    FragColor = vec4(color, 1.0);
}
