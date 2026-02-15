#version 450

// ============================================================================
// Screen Rain Vertex Shader — Fullscreen Triangle
//
// Generates a single triangle that covers the entire screen without
// any vertex buffer. Uses gl_VertexIndex to compute clip-space positions.
// ============================================================================

layout(location = 0) out vec2 fragUV;

void main() {
    // Fullscreen triangle trick: 3 vertices cover entire viewport
    //   Vertex 0: (-1, -1)  bottom-left
    //   Vertex 1: ( 3, -1)  far right
    //   Vertex 2: (-1,  3)  far top
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);

    // Screen UVs: [0,0] at top-left, [1,1] at bottom-right
    // Flip Y so (0,0) is top of screen (rain falls down)
    fragUV = vec2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);
}
