#version 450

layout(location = 0) out vec4 FragColor;

layout(push_constant, std430) uniform OutlineFragPush {
    vec3 u_OutlineColor;
} pc;

void main() {
    FragColor = vec4(pc.u_OutlineColor, 1.0);
}
