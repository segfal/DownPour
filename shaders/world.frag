#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(vNormal);
    float light = max(dot(n, normalize(vec3(0.3, 0.7, 0.2))), 0.15);
    outColor = vec4(vec3(0.2, 0.4, 0.8) * light, 1.0);
}
