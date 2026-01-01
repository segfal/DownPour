#version 450

  layout(set = 0, binding = 0) uniform CameraUBO {
      mat4 view;
      mat4 projection;
      mat4 viewProjection;
  } camera;

  layout(location = 0) in vec3 inPosition;
  layout(location = 1) in vec3 inNormal;
  layout(location = 2) in vec2 inTexCoord;

  layout(location = 0) out vec3 fragNormal;
  layout(location = 1) out vec2 fragTexCoord;

  void main() {
      gl_Position = camera.viewProjection * vec4(inPosition, 1.0);
      fragNormal = inNormal;
      fragTexCoord = inTexCoord;
  }