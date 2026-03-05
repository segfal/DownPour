#version 450

// ============================================================================
// G-Buffer Vertex Shader
//
// Transforms geometry to clip space and passes world-space attributes
// to the G-Buffer fragment shaders. Identical to world.vert — shared by
// road, car, and terrain G-Buffer passes.
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

layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = camera.viewProjection * worldPos;

    // Transform normal by inverse transpose of model matrix (handles non-uniform scale)
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * inNormal);

    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
}
