#version 450

// ============================================================================
// Windshield Vertex Shader
//
// Based on car.vert, extended with:
//   - 5 extra push constant floats (wiper/rain state)
//   - fragScreenUV: screen-space position [0,1] for sampling the scene snapshot
// ============================================================================

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;  // inverse(viewProjection) for depth-to-world reconstruction
    vec4 sunDirection;    // xyz = dir, w = intensity
    vec4 cameraPosition;  // xyz = pos, w = time
    vec4 weatherParams;   // x = rainIntensity, y = wetness, z = windX, w = windZ
} camera;

layout(push_constant) uniform PushConstants {
    mat4  model;
    float alphaMultiplier;
    float wiperAngle;     // degrees, -45 .. +45
    float rainIntensity;  // 0..1
    float carSpeed;       // game units / second
    float time;           // elapsed seconds
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out float fragDistance;
layout(location = 4) out vec2 fragScreenUV;  // [0,1] screen-space UV for snapshot sampling

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    vec4 clipPos  = camera.viewProjection * worldPos;
    gl_Position   = clipPos;

    // Transform normal
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * inNormal);

    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
    fragDistance = length(camera.cameraPosition.xyz - worldPos.xyz);

    // Screen-space UV: NDC xy = clipPos.xy / clipPos.w, remap [-1,1] → [0,1]
    // Note: Vulkan NDC has Y pointing down, so no flip needed here.
    fragScreenUV = (clipPos.xy / clipPos.w) * 0.5 + 0.5;
}
