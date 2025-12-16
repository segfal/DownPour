#version 450

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample the texture
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Daylight sun direction (high overhead, slightly to the side)
    vec3 lightDir = normalize(vec3(0.3, 0.8, 0.4));
    vec3 normal = normalize(fragNormal);

    // Diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);

    // Brighter ambient for daylight scene
    vec3 ambient = 0.5 * texColor.rgb;
    vec3 diffuse = 0.8 * diff * texColor.rgb;

    // Add subtle rim light for depth (simulates sky light)
    vec3 viewDir = normalize(vec3(0.0, 1.0, 0.0) - fragPosition);
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    vec3 rimLight = 0.15 * rim * vec3(0.7, 0.8, 1.0); // Blue-ish sky color

    vec3 finalColor = ambient + diffuse + rimLight;

    // AGGRESSIVE TEST: Force alpha to 0.3 for ALL transparent materials
    // This confirms if the shader is even being used
    float alpha = 0.3;

    outColor = vec4(finalColor, alpha);
}
