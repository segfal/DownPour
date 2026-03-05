#version 450

// ============================================================================
// G-Buffer Debug Visualization Shader
//
// Splits the screen into a 2x2 grid showing individual G-Buffer channels:
//   Top-left:     Albedo
//   Top-right:    World-space Normals
//   Bottom-left:  Roughness (grayscale)
//   Bottom-right: Depth (linearized, grayscale)
//
// Toggle this with a debug key to verify G-Buffer encoding/decoding.
// ============================================================================

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;
    vec4 sunDirection;
    vec4 cameraPosition;
    vec4 weatherParams;
} camera;

layout(set = 2, binding = 0) uniform sampler2D gAlbedoMetallic;
layout(set = 2, binding = 1) uniform sampler2D gNormalRoughness;
layout(set = 2, binding = 2) uniform sampler2D gEmissiveAO;
layout(set = 2, binding = 3) uniform sampler2D gDepth;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    // Determine which quadrant we're in
    vec2 quadrant = step(0.5, fragUV);  // 0 or 1 for each axis
    vec2 localUV = fract(fragUV * 2.0);  // UV within the quadrant [0,1]

    vec3 color;

    if (quadrant.x < 0.5 && quadrant.y < 0.5) {
        // Top-left: Albedo
        color = texture(gAlbedoMetallic, localUV).rgb;
    }
    else if (quadrant.x >= 0.5 && quadrant.y < 0.5) {
        // Top-right: World-space normals (encoded as 0.5+0.5*N, so display directly)
        color = texture(gNormalRoughness, localUV).rgb;
    }
    else if (quadrant.x < 0.5 && quadrant.y >= 0.5) {
        // Bottom-left: Roughness (grayscale)
        float roughness = texture(gNormalRoughness, localUV).a;
        color = vec3(roughness);
    }
    else {
        // Bottom-right: Depth (linearized for visibility)
        float depth = texture(gDepth, localUV).r;
        // Simple linearization for visualization (near=white, far=black)
        float linearDepth = 0.1 / (1.0 - depth + 0.001);  // Approximate near-plane linear
        color = vec3(1.0 - clamp(linearDepth * 0.0001, 0.0, 1.0));
    }

    // Draw grid lines
    vec2 gridDist = abs(fragUV - 0.5);
    float gridLine = step(min(gridDist.x, gridDist.y), 0.002);
    color = mix(color, vec3(1.0, 0.3, 0.0), gridLine);

    outColor = vec4(color, 1.0);
}
