#version 330 core

in float vAO;
out vec4 FragColor;

uniform float u_time_fraction; // Added uniform for time of day

float remap(float value, float inMin, float inMax, float outMin, float outMax) {
    return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
}

void main()
{
    vec3 baseColor = vec3(0.9, 0.9, 0.9);

    // Calculate sun height based on time_fraction
    float ang = u_time_fraction * 2.0 * 3.14159265359 - 3.14159265359 / 2.0;
    float h = sin(ang); // Sun height, 0 at horizon, 1 at noon, -1 at midnight

    // Calculate daylight factor (similar to sunVis for blocks)
    float daylight = clamp(remap(h, -0.02, 0.10, 0.0, 1.0), 0.0, 1.0);

    FragColor = vec4(baseColor * vAO * daylight, 1.0);
}
