#version 450

// ============================================================================
// Deferred Lighting Vertex Shader — Fullscreen Triangle
//
// Same technique as screen_rain.vert: 3 vertices cover the entire viewport
// without any vertex buffer. The fragment shader reads G-Buffer textures.
// ============================================================================

layout(location = 0) out vec2 fragUV;

void main() {
    // Fullscreen triangle: 3 vertices cover entire viewport
    //   Vertex 0: (-1, -1)  bottom-left     UV (0, 1)
    //   Vertex 1: ( 3, -1)  far right       UV (2, 1)
    //   Vertex 2: (-1,  3)  far top         UV (0,-1)
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);

    // UV [0,1] range — (0,0) at top-left to match Vulkan convention
    fragUV = pos * 0.5 + 0.5;
}
