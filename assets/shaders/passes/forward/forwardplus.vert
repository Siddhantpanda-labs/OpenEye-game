#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_WorldPos;
out vec3 v_ViewPos;
out vec3 v_WorldNormal;
out vec2 v_TexCoord;

void main() {
    vec4 w = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = w.xyz;
    vec4 viewPos = u_View * w;
    v_ViewPos = viewPos.xyz;
    v_WorldNormal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_TexCoord = a_TexCoord;
    gl_Position = u_Projection * viewPos;
}
