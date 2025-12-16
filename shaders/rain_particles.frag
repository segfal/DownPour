#version 450

// TODO: Implement rain particle fragment shader
// This shader will render rain droplets falling through the air
// Effects to implement:
// - Semi-transparent elongated droplets
// - Motion blur for fast-moving rain
// - Lighting/reflection on droplets

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Placeholder: render as light blue semi-transparent particles
    // TODO: Add proper droplet texture/shape
    // TODO: Add atmospheric scattering for distant rain
    // TODO: Add variation in droplet size and intensity
    
    outColor = vec4(0.7, 0.8, 0.9, 0.3); // Light blue, semi-transparent
}
