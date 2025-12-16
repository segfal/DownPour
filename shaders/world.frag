  #version 450

  layout(location = 0) in vec3 fragNormal;
  layout(location = 1) in vec2 fragTexCoord;

  layout(location = 0) out vec4 outColor;

  void main() {
      // Simple lighting based on normal
      vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
      float diff = max(dot(fragNormal, lightDir), 0.0);
      vec3 roadColor = vec3(0.3, 0.3, 0.3);  // Dark grey road
      vec3 color = roadColor * (0.3 + 0.7 * diff);  // Ambient + diffuse
      outColor = vec4(color, 1.0);
  }
