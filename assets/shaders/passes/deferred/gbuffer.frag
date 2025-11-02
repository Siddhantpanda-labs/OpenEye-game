#version 450 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;

layout(location = 0) out vec3 g_Position;
layout(location = 1) out vec3 g_Normal;
layout(location = 2) out vec3 g_Albedo;
layout(location = 3) out vec2 g_PBRParams;

uniform vec3 u_Albedo = vec3(0.8);
uniform float u_Metallic = 0.0;
uniform float u_Roughness = 0.5;

uniform sampler2D u_AlbedoTexture;
uniform bool u_HasAlbedoTexture = false;

void main() {
    g_Position = v_Position;
    vec3 normal = normalize(v_Normal);
    if (isnan(normal.x) || isnan(normal.y) || isnan(normal.z)) normal = vec3(0.0, 1.0, 0.0);
    g_Normal = normal;
    vec3 albedoColor = u_Albedo;
    if (u_HasAlbedoTexture) albedoColor = texture(u_AlbedoTexture, v_TexCoord).rgb * u_Albedo;
    g_Albedo = clamp(albedoColor, vec3(0.0), vec3(1.0));
    g_PBRParams = vec2(clamp(u_Metallic, 0.0, 1.0), clamp(u_Roughness, 0.04, 1.0));
}


