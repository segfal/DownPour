#version 450

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;  // inverse(viewProjection) for depth-to-world reconstruction
    vec4 sunDirection;    // xyz = dir, w = intensity
    vec4 cameraPosition;  // xyz = pos, w = time
    vec4 weatherParams;   // x = rainIntensity, y = wetness, z = windX, w = windZ
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out float fragDistance;

void main() {
    gl_Position = camera.viewProjection * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragTexCoord = inTexCoord;
    fragWorldPos = inPosition;

    // Calculate distance to camera for fog
    fragDistance = length(camera.cameraPosition.xyz - inPosition);
}
