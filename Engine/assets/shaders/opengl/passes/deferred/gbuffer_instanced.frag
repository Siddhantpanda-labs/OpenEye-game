#version 460 core
@MATERIAL_DEFINES@

in vec3 v_WorldPos;
in vec3 v_WorldNormal;
in vec2 v_TexCoord;
in vec2 v_Velocity;
in mat3 v_TBN;

layout(location = 0) out vec4 g_Position;
layout(location = 1) out vec4 g_Normal;
layout(location = 2) out vec4 g_Albedo;
layout(location = 3) out vec3 g_PBRParams;
layout(location = 4) out vec2 g_Velocity;

// Refractor Phase 2: material props from UBO (binding 2 = PerMaterial)
layout(std140, binding = 2) uniform MaterialBlock {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float alpha;
    vec3 emission;
    float reflective;
};

#ifdef HAS_ALBEDO_TEXTURE
uniform sampler2D u_AlbedoTexture;
#endif
#ifdef HAS_NORMAL_MAP
uniform sampler2D u_NormalTexture;
#endif
#ifdef HAS_METALLIC_TEXTURE
uniform sampler2D u_MetallicTexture;
#endif
#ifdef HAS_ROUGHNESS_TEXTURE
uniform sampler2D u_RoughnessTexture;
#endif
#ifdef HAS_AO_TEXTURE
uniform sampler2D u_AOTexture;
#endif
#ifdef HAS_EMISSIVE_TEXTURE
uniform sampler2D u_EmissiveTexture;
#endif
#ifdef HAS_METALLIC_ROUGHNESS_TEXTURE
uniform sampler2D u_MetallicRoughnessTexture;
#endif

// Fallback when not using property defines (default material path)
#ifndef HAS_ALBEDO_TEXTURE
uniform sampler2D u_AlbedoTexture;
#endif
#ifndef HAS_NORMAL_MAP
uniform sampler2D u_NormalTexture;
#endif
#ifndef HAS_METALLIC_TEXTURE
uniform sampler2D u_MetallicTexture;
#endif
#ifndef HAS_ROUGHNESS_TEXTURE
uniform sampler2D u_RoughnessTexture;
#endif
#ifndef HAS_AO_TEXTURE
uniform sampler2D u_AOTexture;
#endif
#ifndef HAS_EMISSIVE_TEXTURE
uniform sampler2D u_EmissiveTexture;
#endif
#ifndef HAS_METALLIC_ROUGHNESS_TEXTURE
uniform sampler2D u_MetallicRoughnessTexture;
#endif

uniform float u_HasAlbedoTexture = 0.0;
uniform float u_HasNormalTexture = 0.0;
uniform float u_HasMetallicTexture = 0.0;
uniform float u_HasRoughnessTexture = 0.0;
uniform float u_HasAOTexture = 0.0;
uniform float u_HasMetallicRoughnessTexture = 0.0;

void main() {
    @MATERIAL_FRAGMENT_SNIPPET@
    g_Position = vec4(v_WorldPos, 1.0);

    vec3 normal = normalize(v_WorldNormal);
#ifdef HAS_NORMAL_MAP
    vec3 normalSample = texture(u_NormalTexture, v_TexCoord).rgb;
    normalSample = normalSample * 2.0 - 1.0;
    normal = normalize(v_TBN * normalSample);
#else
    if (u_HasNormalTexture > 0.5) {
        vec3 normalSample = texture(u_NormalTexture, v_TexCoord).rgb;
        normalSample = normalSample * 2.0 - 1.0;
        normal = normalize(v_TBN * normalSample);
    }
#endif
    if (isnan(normal.x) || isnan(normal.y) || isnan(normal.z)) normal = vec3(0.0, 1.0, 0.0);
    g_Normal = vec4(normal, 1.0);

    vec3 albedoColor = albedo;
#ifdef HAS_ALBEDO_TEXTURE
    albedoColor = texture(u_AlbedoTexture, v_TexCoord).rgb * albedo;
#else
    if (u_HasAlbedoTexture > 0.5) {
        albedoColor = texture(u_AlbedoTexture, v_TexCoord).rgb * albedo;
    }
#endif

#if defined(SHADING_UNLIT)
    g_Albedo = vec4(clamp(albedoColor + emission, vec3(0.0), vec3(1.0)), 1.0);
    g_PBRParams = vec3(2.0, 1.0, 0.0);  // metallic=2.0: lighting pass treats as unlit
#else
    g_Albedo = vec4(clamp(albedoColor, vec3(0.0), vec3(1.0)), 1.0);

    float metallicVal = metallic;
    float roughnessVal = roughness;
#ifdef HAS_METALLIC_ROUGHNESS_TEXTURE
    vec2 mr = texture(u_MetallicRoughnessTexture, v_TexCoord).bg;
    metallicVal = mix(metallicVal, mr.x, 1.0);
    roughnessVal = mix(roughnessVal, mr.y, 1.0);
#else
    if (u_HasMetallicRoughnessTexture > 0.5) {
        vec2 mr = texture(u_MetallicRoughnessTexture, v_TexCoord).bg;
        metallicVal = mix(metallicVal, mr.x, 1.0);
        roughnessVal = mix(roughnessVal, mr.y, 1.0);
    } else {
#ifdef HAS_METALLIC_TEXTURE
        metallicVal = texture(u_MetallicTexture, v_TexCoord).r;
#else
        if (u_HasMetallicTexture > 0.5) metallicVal = texture(u_MetallicTexture, v_TexCoord).r;
#endif
#ifdef HAS_ROUGHNESS_TEXTURE
        roughnessVal = texture(u_RoughnessTexture, v_TexCoord).r;
#else
        if (u_HasRoughnessTexture > 0.5) roughnessVal = texture(u_RoughnessTexture, v_TexCoord).r;
#endif
    }
#endif

    g_PBRParams = vec3(clamp(metallicVal, 0.0, 1.0), clamp(roughnessVal, 0.04, 1.0), clamp(reflective, 0.0, 1.0));
#endif

    g_Velocity = v_Velocity;
}
