#version 430 core

in vec2 v_TexCoord;

layout(binding = 0) uniform sampler2D u_Position;
layout(binding = 1) uniform sampler2D u_Normal;
layout(binding = 2) uniform sampler2D u_Albedo;
layout(binding = 3) uniform sampler2D u_PBR;

uniform vec3 u_ViewPos;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform vec3 u_Ambient;
uniform float u_Exposure;

layout(location = 0) out vec4 o_Color;

void main() {
    vec3 pos = texture(u_Position, v_TexCoord).rgb;
    vec3 N = normalize(texture(u_Normal, v_TexCoord).rgb);
    vec3 albedo = texture(u_Albedo, v_TexCoord).rgb;
    vec2 mr = texture(u_PBR, v_TexCoord).rg;
    float metallic = mr.r;
    float roughness = max(mr.g, 0.04);

    vec3 V = normalize(u_ViewPos - pos);
    vec3 L = normalize(-u_LightDir);
    float NdotL = max(dot(N, L), 0.0);

    vec3 diff = albedo * (1.0 - metallic) * u_Ambient;
    diff += albedo * u_LightColor * u_LightIntensity * NdotL;

    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    diff += (0.04 + albedo * metallic) * u_LightColor * u_LightIntensity * spec;

    o_Color = vec4(diff * u_Exposure, 1.0);
}
