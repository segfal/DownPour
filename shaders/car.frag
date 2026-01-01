#version 450

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample the texture
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Simple directional light
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 normal = normalize(fragNormal);

    // Diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);

    // Ambient + diffuse
    vec3 ambient = 0.3 * texColor.rgb;
    vec3 diffuse = 0.7 * diff * texColor.rgb;

    vec3 finalColor = ambient + diffuse;

    outColor = vec4(finalColor, texColor.a);
}
