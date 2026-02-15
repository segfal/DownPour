#version 450

// ============================================================================
// Rain Particle Fragment Shader — Thin Realistic Streaks (1000x scale)
// ============================================================================

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragAlpha;
layout(location = 2) in float fragDistance;

layout(location = 0) out vec4 outColor;

void main() {
    // Horizontal: sharp thin core (rain streaks are very narrow)
    float dx = abs(fragUV.x - 0.5) * 2.0;
    float core = exp(-dx * dx * 20.0);  // Very sharp horizontal falloff

    // Vertical: tapers to points at both ends
    float dy = abs(fragUV.y - 0.5) * 2.0;
    float taper = 1.0 - pow(dy, 3.0);  // Cubic taper — sharper tips than quadratic
    taper = max(taper, 0.0);

    float alpha = core * taper * fragAlpha;

    // Distance fade (scaled for 1000x)
    float distFade = exp(-0.000006 * fragDistance);
    alpha *= distFade;

    // Subtle semi-transparent rain color — not glowing white
    vec3 rainColor = vec3(0.7, 0.72, 0.78);

    outColor = vec4(rainColor, alpha * 0.35);
}
