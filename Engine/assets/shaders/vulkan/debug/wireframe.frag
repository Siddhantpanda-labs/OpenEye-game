#version 460

layout(location = 0) out vec4 FragColor;

layout(push_constant, std430) uniform WireframeFragPush {
    vec3 u_Color;
    float _pad;
} pc;

void main() {
    FragColor = vec4(pc.u_Color, 1.0);
}
