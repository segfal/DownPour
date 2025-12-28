#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
} cam;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
} obj;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;

void main() {
    gl_Position = cam.viewProj * obj.model * vec4(inPos, 1.0);
    vNormal = mat3(obj.model) * inNormal;
    vUV = inUV;
}
