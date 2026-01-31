#version 430 core

in vec2 v_TexCoord;

layout(binding = 0) uniform sampler2D u_Position;
layout(binding = 1) uniform sampler2D u_Normal;
layout(binding = 2) uniform sampler2D u_Albedo;
layout(binding = 3) uniform sampler2D u_PBR;
uniform sampler2D u_ShadowMap;

uniform vec3 u_ViewPos;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform vec3 u_Ambient;
uniform float u_Exposure;
uniform mat4 u_LightSpaceMatrix;
uniform float u_ShadowBias;
uniform int u_UseShadows;

layout(location = 0) out vec4 o_Color;

float shadowFactor(vec3 worldPos) {
    vec4 lightSpacePos = u_LightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
    ndc = ndc * 0.5 + 0.5;
    if (ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0)
        return 0.0;
    float depth = texture(u_ShadowMap, ndc.xy).r;
    return (ndc.z - u_ShadowBias > depth) ? 1.0 : 0.0;
}

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

    float shadow = 0.0;
    if (u_UseShadows > 0)
        shadow = shadowFactor(pos);
    float lightVis = 1.0 - shadow;

    vec3 diff = albedo * (1.0 - metallic) * u_Ambient;
    diff += albedo * u_LightColor * u_LightIntensity * NdotL * lightVis;

    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    diff += (0.04 + albedo * metallic) * u_LightColor * u_LightIntensity * spec * lightVis;

    o_Color = vec4(diff * u_Exposure, 1.0);
}
