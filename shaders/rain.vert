#version 450

// ============================================================================
// Rain Particle Vertex Shader — Velocity-Stretched Streaks (1000x scale)
// ============================================================================

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;  // inverse(viewProjection) for depth-to-world reconstruction
    vec4 sunDirection;
    vec4 cameraPosition;
    vec4 weatherParams;
} camera;

layout(location = 0) in vec2 inCorner;
layout(location = 1) in vec3 inPosition;
layout(location = 2) in float inAlpha;
layout(location = 3) in vec3 inVelocity;
layout(location = 4) in float inSize;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out float fragAlpha;
layout(location = 2) out float fragDistance;
layout(location = 3) out float fragSize;

void main() {
    vec3 velDir = normalize(inVelocity);
    float speed = length(inVelocity);

    vec3 toCamera = normalize(camera.cameraPosition.xyz - inPosition);
    vec3 right = normalize(cross(velDir, toCamera));

    // At 1000x scale, drops are size 1-3.
    // Real rain: ~2mm wide, 5-20cm long streak at typical shutter speed.
    // At 1000x: width ~2 units, length ~50-200 units.
    float halfWidth  = inSize * 0.8;
    float halfLength = inSize * 30.0 + speed * 0.008;

    vec3 worldPos = inPosition
                  + right  * inCorner.x * halfWidth
                  + velDir * inCorner.y * halfLength;

    gl_Position = camera.viewProjection * vec4(worldPos, 1.0);

    fragUV = inCorner * 0.5 + 0.5;
    fragAlpha = inAlpha;
    fragDistance = length(camera.cameraPosition.xyz - inPosition);
    fragSize = inSize;
}
