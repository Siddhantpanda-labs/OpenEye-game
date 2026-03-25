#version 450

layout(location = 0) in vec3 a_Position;

layout(push_constant, std430) uniform EntityIdPush {
    layout(offset = 0) mat4 u_Model;
    layout(offset = 64) mat4 u_View;
    layout(offset = 128) mat4 u_Projection;
} pc;

void main() {
    vec4 worldPos = pc.u_Model * vec4(a_Position, 1.0);
    gl_Position = pc.u_Projection * pc.u_View * worldPos;
}
