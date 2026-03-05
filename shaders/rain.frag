#version 450

// ============================================================================
// Rain Particle Fragment Shader — Thin Realistic Streaks (1000x scale)
// ============================================================================

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragAlpha;
layout(location = 2) in float fragDistance;
// instanceSize is passed as a third per-instance attribute via the vertex shader
layout(location = 3) in float fragSize;

layout(location = 0) out vec4 outColor;

void main() {
    // Streak width proportional to drop size (larger/closer drops are wider)
    // coreWidth: 0.008 for tiny far drops, 0.022 for large near drops
    float coreWidth = mix(0.008, 0.022, clamp(fragSize / 3.0, 0.0, 1.0));
    float sharpness = 1.0 / (coreWidth * coreWidth);

    // Horizontal: sharp core, width varies with drop size
    float dx = abs(fragUV.x - 0.5) * 2.0;
    float core = exp(-dx * dx * sharpness);

    // Vertical: cubic taper — slightly longer bright core for larger drops
    float dy = abs(fragUV.y - 0.5) * 2.0;
    float taper = 1.0 - pow(dy, 3.0);
    taper = max(taper, 0.0);

    float alpha = core * taper * fragAlpha;

    // Distance fade (scaled for 1000x world)
    float distFade = exp(-0.000006 * fragDistance);
    alpha *= distFade;

    // Color gradient: brighter/whiter at leading edge (top), bluer toward trailing edge (bottom)
    // fragUV.y = 0 is the leading tip, 1 is the trailing tip
    vec3 leadColor  = vec3(0.82, 0.84, 0.90);  // bright white-blue at tip
    vec3 trailColor = vec3(0.55, 0.60, 0.72);  // darker blue at tail
    vec3 rainColor = mix(leadColor, trailColor, fragUV.y);

    outColor = vec4(rainColor, alpha * 0.35);
}
