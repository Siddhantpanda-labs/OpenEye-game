// OpenEye next-gen rendering — sky pass (cubemap variant) vertex shader.
// Generates a full-screen triangle from gl_VertexIndex (no VBO). vUv ∈ [0,1] covers the
// fragment area exactly once; the fragment shader reconstructs a world-space view ray
// from vUv and the inverse view/projection push constants.
//
// Logical asset path: passes/sky/skybox_cubemap
// Layout: PipelineLayoutKind::IBL (set 0 binding 0 = samplerCube). Push constants:
//   offset  0  mat4 uInverseView
//   offset 64  mat4 uInverseProjection
//   offset 128 float uSkyboxBrightness  (rest of the 256-byte range unused)

#version 450

layout(location = 0) out vec2 vUv;

void main() {
    vUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    // z = 1.0 places the sky at the far plane so depth-test LessEqual lets opaque geometry occlude it.
    gl_Position = vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
}
