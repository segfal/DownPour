#version 450

// TODO: Implement rain particle vertex shader
// This shader will handle the transformation of rain droplet particles
// Inputs will include:
// - Particle position (world space)
// - Particle velocity
// - Particle lifetime/size

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
} camera;

void main() {
    // Placeholder: simple passthrough
    // TODO: Add particle billboard rotation to face camera
    // TODO: Add particle size scaling based on lifetime
    // TODO: Add wind effect displacement
    
    gl_Position = camera.viewProj * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}
