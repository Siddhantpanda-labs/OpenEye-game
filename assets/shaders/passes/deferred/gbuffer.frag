#version 450 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;
in mat3 v_TBN;

layout(location = 0) out vec3 g_Position;
layout(location = 1) out vec3 g_Normal;
layout(location = 2) out vec3 g_Albedo;
layout(location = 3) out vec2 g_PBRParams;

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

uniform bool u_HasAlbedoTexture = false;
uniform bool u_HasNormalTexture = false;
uniform bool u_HasMetallicTexture = false;
uniform bool u_HasRoughnessTexture = false;
uniform bool u_HasAOTexture = false;
uniform bool u_HasMetallicRoughnessTexture = false;

void main() {
    g_Position = v_Position;
    
    // Normal mapping
    vec3 normal = normalize(v_Normal);
    if (u_HasNormalTexture) {
        vec3 normalSample = texture(u_NormalTexture, v_TexCoord).rgb;
        normalSample = normalSample * 2.0 - 1.0; // Convert from [0,1] to [-1,1]
        normal = normalize(v_TBN * normalSample);
    }
    if (isnan(normal.x) || isnan(normal.y) || isnan(normal.z)) normal = vec3(0.0, 1.0, 0.0);
    g_Normal = normal;
    
    // Albedo
    vec3 albedoColor = u_Albedo;
    if (u_HasAlbedoTexture) {
        albedoColor = texture(u_AlbedoTexture, v_TexCoord).rgb * u_Albedo;
    }
    g_Albedo = clamp(albedoColor, vec3(0.0), vec3(1.0));
    
    // Metallic and Roughness
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_HasMetallicRoughnessTexture) {
        vec2 mr = texture(u_MetallicRoughnessTexture, v_TexCoord).bg; // B=metallic, G=roughness (GLTF convention)
        metallic = mix(metallic, mr.x, 1.0);
        roughness = mix(roughness, mr.y, 1.0);
    } else {
        if (u_HasMetallicTexture) metallic = texture(u_MetallicTexture, v_TexCoord).r;
        if (u_HasRoughnessTexture) roughness = texture(u_RoughnessTexture, v_TexCoord).r;
    }
    
    // AO (pack into PBRParams if needed, or use separate channel)
    float ao = u_AO;
    if (u_HasAOTexture) ao = texture(u_AOTexture, v_TexCoord).r;
    
    g_PBRParams = vec2(clamp(metallic, 0.0, 1.0), clamp(roughness, 0.04, 1.0));
}


