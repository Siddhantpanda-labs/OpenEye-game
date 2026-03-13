#version 460 core
@MATERIAL_DEFINES@

layout(location = 0) out vec4 g_Position;
layout(location = 1) out vec4 g_Normal;
layout(location = 2) out vec4 g_Albedo;
layout(location = 3) out vec3 g_PBRParams;
layout(location = 4) out vec2 g_Velocity;

in vec3 v_WorldPos;
in vec3 v_WorldNormal;
in vec2 v_TexCoord;
in vec2 v_Velocity;

// Refractor: material props from UBO (binding 2 = PerMaterial), same layout as gbuffer_skinned
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
#ifndef HAS_ALBEDO_TEXTURE
uniform sampler2D u_AlbedoTexture;
#endif
uniform float u_HasAlbedoTexture = 0.0;

void main() {
    @MATERIAL_FRAGMENT_SNIPPET@
    g_Position = vec4(v_WorldPos, 1.0);
    g_Normal = vec4(normalize(v_WorldNormal), 1.0);

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
    g_PBRParams = vec3(2.0, 1.0, 0.0);
#else
    g_Albedo = vec4(clamp(albedoColor, vec3(0.0), vec3(1.0)), 1.0);
    g_PBRParams = vec3(clamp(metallic, 0.0, 1.0), clamp(roughness, 0.04, 1.0), clamp(reflective, 0.0, 1.0));
#endif

    g_Velocity = v_Velocity;
}
