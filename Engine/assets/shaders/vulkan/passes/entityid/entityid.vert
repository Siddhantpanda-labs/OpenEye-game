#version 450

layout(location = 0) in vec3 a_Position;

layout(push_constant, std430) uniform EntityIdPush {
    mat4 u_Model;
    mat4 u_View;
    mat4 u_Projection;
} pc;

void main() {
    vec4 worldPos = pc.u_Model * vec4(a_Position, 1.0);
    gl_Position = pc.u_Projection * pc.u_View * worldPos;
}
