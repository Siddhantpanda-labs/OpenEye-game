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

#include "../../common/material_block.glsl"

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

#include "../../common/material_surface.glsl"

void main() {
    @MATERIAL_FRAGMENT_SNIPPET@
    g_Position = vec4(v_WorldPos, 1.0);

    bool useAlbedo = false;
#ifdef HAS_ALBEDO_TEXTURE
    useAlbedo = true;
#else
    useAlbedo = (u_HasAlbedoTexture > 0.5);
#endif
    mat3 tbn = mat3(1.0);

    MaterialSurface surf;
    EvaluateMaterialSurface(
        v_TexCoord, tbn, v_WorldNormal,
        MaterialBlock.albedo, MaterialBlock.metallic, MaterialBlock.roughness, MaterialBlock.ao, MaterialBlock.emission, MaterialBlock.alpha, MaterialBlock.reflective,
        u_AlbedoTexture, u_NormalTexture, u_MetallicRoughnessTexture,
        u_MetallicTexture, u_RoughnessTexture, u_AOTexture, u_EmissiveTexture,
        useAlbedo, false, false, false, false, false, false,
        surf
    );

    g_Normal = vec4(surf.normal, 1.0);

#if defined(SHADING_UNLIT)
    g_Albedo = vec4(clamp(surf.albedo + surf.emission, vec3(0.0), vec3(1.0)), 1.0);
    g_PBRParams = vec3(2.0, 1.0, 0.0);
#else
    g_Albedo = vec4(surf.albedo, 1.0);
    g_PBRParams = vec3(surf.metallic, surf.roughness, surf.reflective);
#endif

    g_Velocity = v_Velocity;
}
