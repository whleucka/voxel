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

// Underwater fog settings
const vec3 WATER_FOG_COLOR = vec3(0.1, 0.3, 0.5);
const float WATER_FOG_DENSITY = 0.04;

void main() {
    // Repeat every 1.0 unit in baseUV space (i.e., each block)
    vec2 tileUV  = fract(vBaseUV);              // 0..1 per block
    vec2 atlasUV = vTileOffset + tileUV * vTileSpan;
    vec4 texColor = texture(uTexture, atlasUV);

    vec3 finalColor = texColor.rgb;

    if (uUnderwater) {
        // Calculate distance from camera for fog
        float dist = length(vWorldPos - uCameraPos);

        // Exponential fog factor
        float fogFactor = 1.0 - exp(-WATER_FOG_DENSITY * dist);
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        // Mix texture color with fog color
        finalColor = mix(finalColor, WATER_FOG_COLOR, fogFactor);
    }

    FragColor = vec4(finalColor, texColor.a * uAlpha);
}
