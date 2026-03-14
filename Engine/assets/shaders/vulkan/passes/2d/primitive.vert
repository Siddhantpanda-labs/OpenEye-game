#version 460

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;

layout(location = 0) out vec4 v_Color;

layout(push_constant, std430) uniform PrimitivePush {
    mat4 u_View;
    mat4 u_Projection;
} pc;

void main() {
    v_Color = a_Color;
    gl_Position = pc.u_Projection * pc.u_View * vec4(a_Position, 0.0, 1.0);
}
