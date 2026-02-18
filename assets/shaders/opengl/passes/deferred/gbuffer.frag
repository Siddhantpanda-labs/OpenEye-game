#version 460 core

layout(location = 0) out vec4 g_Position;
layout(location = 1) out vec4 g_Normal;
layout(location = 2) out vec4 g_Albedo;
layout(location = 3) out vec2 g_PBRParams;
layout(location = 4) out vec2 g_Velocity;

in vec3 v_WorldPos;
in vec3 v_WorldNormal;
in vec2 v_TexCoord;
in vec2 v_Velocity;

uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;

void main() {
    g_Position = vec4(v_WorldPos, 1.0);
    g_Normal = vec4(normalize(v_WorldNormal), 1.0);
    g_Albedo = vec4(u_Albedo, 1.0);
    g_PBRParams = vec2(u_Metallic, u_Roughness);
    g_Velocity = v_Velocity;
}
