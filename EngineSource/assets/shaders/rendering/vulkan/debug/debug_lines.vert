// OpenEye next-gen rendering — editor debug-line overlay (line list).
// Logical: debug/debug_lines  |  Layout: ComposeBootstrap (set0 Frame UBOs only; no PerObject SSBO).
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(set = 0, binding = 1) uniform PerViewUbo {
    mat4 uView;
    mat4 uProjection;
    mat4 uViewProjection;
    vec4 uViewPos;
    vec4 uViewport;
    vec4 uViewPad0;
    vec4 uViewPad1;
} uPerView;

layout(location = 0) out vec4 vColor;

void main() {
    vColor = inColor;
    gl_Position = uPerView.uViewProjection * vec4(inPosition, 1.0);
}
