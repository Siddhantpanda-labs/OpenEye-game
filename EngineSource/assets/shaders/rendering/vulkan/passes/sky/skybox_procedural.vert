// OpenEye next-gen rendering — sky pass (procedural variant) vertex shader.
// Same full-screen triangle as the cubemap variant; depth z = 1 sits at the far plane.
//
// Logical asset path: passes/sky/skybox_procedural
// Layout: PipelineLayoutKind::PushOnly (no descriptor sets). Push constants:
//   offset  0  mat4 uInverseView
//   offset 64  mat4 uInverseProjection
//   offset 128 float uSkyboxBrightness  (used as a global gain on the procedural gradient)

#version 450

layout(location = 0) out vec2 vUv;

void main() {
    vUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
}
