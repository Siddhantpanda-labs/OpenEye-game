// OpenEye next-gen rendering — M5 Deferred Lighting full-screen triangle vertex shader.
// Logical: raster/deferred/deferred_lighting_analytic
// Generates a full-screen triangle from gl_VertexIndex (no VBO needed).
#version 450

layout(location = 0) out vec2 vUv;

void main() {
    // Full-screen triangle: vertices 0,1,2 map to (-1,-1), (3,-1), (-1,3) in NDC.
    vUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUv * 2.0 - 1.0, 0.0, 1.0);
}
