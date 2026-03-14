#version 460

#define HAS_ALBEDO_TEXTURE 0
#define HAS_NORMAL_MAP 0
#define HAS_METALLIC_TEXTURE 0
#define HAS_ROUGHNESS_TEXTURE 0
#define HAS_AO_TEXTURE 0
#define HAS_EMISSIVE_TEXTURE 0
#define HAS_METALLIC_ROUGHNESS_TEXTURE 0

layout(location = 0) out vec4 g_Position;
layout(location = 1) out vec4 g_Normal;
layout(location = 2) out vec4 g_Albedo;
layout(location = 3) out vec3 g_PBRParams;
layout(location = 4) out vec2 g_Velocity;

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_WorldNormal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in vec2 v_Velocity;

#include "../../common/material_block.glsl"

layout(binding = 0) uniform sampler2D u_AlbedoTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;
layout(binding = 2) uniform sampler2D u_MetallicRoughnessTexture;
layout(binding = 3) uniform sampler2D u_MetallicTexture;
layout(binding = 4) uniform sampler2D u_RoughnessTexture;
layout(binding = 5) uniform sampler2D u_AOTexture;
layout(binding = 6) uniform sampler2D u_EmissiveTexture;

layout(push_constant, std430) uniform GBufferFragPush {
    float u_HasAlbedoTexture;
} pc;

#include "../../common/material_surface.glsl"

void main() {
    g_Position = vec4(v_WorldPos, 1.0);
    bool useAlbedo = (pc.u_HasAlbedoTexture > 0.5);
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
    g_Albedo = vec4(surf.albedo, 1.0);
    g_PBRParams = vec3(surf.metallic, surf.roughness, surf.reflective);
    g_Velocity = v_Velocity;
}
