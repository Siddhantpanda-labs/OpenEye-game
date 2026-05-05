#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;

layout(push_constant) uniform PushData {
    mat4 u_MVP;
    mat4 u_Model;
} pc;

void main() {
    vec4 world = pc.u_Model * vec4(a_Position, 1.0);
    gl_Position = pc.u_MVP * vec4(a_Position, 1.0);
    v_WorldPos = world.xyz;
    mat3 normalMat = mat3(transpose(inverse(pc.u_Model)));
    v_Normal = normalize(normalMat * a_Normal);
}
