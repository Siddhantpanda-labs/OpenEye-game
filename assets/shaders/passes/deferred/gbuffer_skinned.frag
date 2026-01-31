#version 450 core

in vec3 v_WorldPos;
in vec3 v_WorldNormal;
in vec2 v_TexCoord;
in vec2 v_Velocity;
in mat3 v_TBN;

layout(location = 0) out vec4 g_Position;
layout(location = 1) out vec4 g_Normal;
layout(location = 2) out vec4 g_Albedo;
layout(location = 3) out vec2 g_PBRParams;
layout(location = 4) out vec2 g_Velocity;

uniform vec3 u_Albedo = vec3(0.8);
uniform float u_Metallic = 0.0;
uniform float u_Roughness = 0.5;
uniform float u_AO = 1.0;

uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_MetallicTexture;
uniform sampler2D u_RoughnessTexture;
uniform sampler2D u_AOTexture;
uniform sampler2D u_MetallicRoughnessTexture; // Combined (R=metallic, G=roughness)

uniform float u_HasAlbedoTexture = 0.0;
uniform float u_HasNormalTexture = 0.0;
uniform float u_HasMetallicTexture = 0.0;
uniform float u_HasRoughnessTexture = 0.0;
uniform float u_HasAOTexture = 0.0;
uniform float u_HasMetallicRoughnessTexture = 0.0;

void main() {
    g_Position = vec4(v_WorldPos, 1.0);
    
    // Normal mapping (same as forward skinned.frag + TBN)
    vec3 normal = normalize(v_WorldNormal);
    if (u_HasNormalTexture > 0.5) {
        vec3 normalSample = texture(u_NormalTexture, v_TexCoord).rgb;
        normalSample = normalSample * 2.0 - 1.0;
        normal = normalize(v_TBN * normalSample);
    }
    if (isnan(normal.x) || isnan(normal.y) || isnan(normal.z)) normal = vec3(0.0, 1.0, 0.0);
    g_Normal = vec4(normal, 1.0);
    
    // Albedo (same as forward skinned)
    vec3 albedoColor = u_Albedo;
    if (u_HasAlbedoTexture > 0.5) {
        albedoColor = texture(u_AlbedoTexture, v_TexCoord).rgb * u_Albedo;
    }
    g_Albedo = vec4(clamp(albedoColor, vec3(0.0), vec3(1.0)), 1.0);
    
    // Metallic and Roughness (same as forward skinned / glTF)
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_HasMetallicRoughnessTexture > 0.5) {
        vec2 mr = texture(u_MetallicRoughnessTexture, v_TexCoord).bg; // B=metallic, G=roughness
        metallic = mix(metallic, mr.x, 1.0);
        roughness = mix(roughness, mr.y, 1.0);
    } else {
        if (u_HasMetallicTexture > 0.5) metallic = texture(u_MetallicTexture, v_TexCoord).r;
        if (u_HasRoughnessTexture > 0.5) roughness = texture(u_RoughnessTexture, v_TexCoord).r;
    }
    
    g_PBRParams = vec2(clamp(metallic, 0.0, 1.0), clamp(roughness, 0.04, 1.0));
    g_Velocity = v_Velocity;
}

