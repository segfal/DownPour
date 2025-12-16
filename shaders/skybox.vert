#version 450
layout(location = 0) in vec3 inPos;
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
} cam;
void main() {
    gl_Position = cam.viewProj * vec4(inPos, 1.0);
}
