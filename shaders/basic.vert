#version 450

  layout(set = 0, binding = 0) uniform CameraUBO {
      mat4 view;
      mat4 projection;
      mat4 viewProjection;
  } camera;

  // Hardcoded cube vertices (36 vertices for 6 faces)
  vec3 positions[36] = vec3[](
      // Front face
      vec3(-1.0, -1.0,  1.0), vec3( 1.0, -1.0,  1.0), vec3( 1.0,  1.0,  1.0),
      vec3( 1.0,  1.0,  1.0), vec3(-1.0,  1.0,  1.0), vec3(-1.0, -1.0,  1.0),
      // Back face
      vec3( 1.0, -1.0, -1.0), vec3(-1.0, -1.0, -1.0), vec3(-1.0,  1.0, -1.0),
      vec3(-1.0,  1.0, -1.0), vec3( 1.0,  1.0, -1.0), vec3( 1.0, -1.0, -1.0),
      // Top face
      vec3(-1.0,  1.0,  1.0), vec3( 1.0,  1.0,  1.0), vec3( 1.0,  1.0, -1.0),
      vec3( 1.0,  1.0, -1.0), vec3(-1.0,  1.0, -1.0), vec3(-1.0,  1.0,  1.0),
      // Bottom face
      vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0),
      vec3( 1.0, -1.0,  1.0), vec3(-1.0, -1.0,  1.0), vec3(-1.0, -1.0, -1.0),
      // Right face
      vec3( 1.0, -1.0,  1.0), vec3( 1.0, -1.0, -1.0), vec3( 1.0,  1.0, -1.0),
      vec3( 1.0,  1.0, -1.0), vec3( 1.0,  1.0,  1.0), vec3( 1.0, -1.0,  1.0),
      // Left face
      vec3(-1.0, -1.0, -1.0), vec3(-1.0, -1.0,  1.0), vec3(-1.0,  1.0,  1.0),
      vec3(-1.0,  1.0,  1.0), vec3(-1.0,  1.0, -1.0), vec3(-1.0, -1.0, -1.0)
  );

  layout(location = 0) out vec3 fragColor;

  void main() {
      vec3 pos = positions[gl_VertexIndex] * 100.0;  // Large skybox
      gl_Position = camera.viewProjection * vec4(pos, 1.0);

      // Gradient sky color (darker at bottom, lighter at top)
      float t = (positions[gl_VertexIndex].y + 1.0) * 0.5;  // 0 to 1
      vec3 skyBottom = vec3(0.3, 0.4, 0.6);  // Blue-grey
      vec3 skyTop = vec3(0.5, 0.7, 1.0);     // Light blue
      fragColor = mix(skyBottom, skyTop, t);
  }