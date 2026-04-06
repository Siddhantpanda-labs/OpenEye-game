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
layout(location = 4) in mat3 v_TBN;

// Set 1 = Material (binding 0 UBO, 1–3 textures)
layout(set = 1, binding = 0) uniform MaterialParams {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float alpha;
    vec3 emission;
    float reflective;
} MaterialBlock;

layout(set = 1, binding = 1) uniform sampler2D u_AlbedoTexture;
layout(set = 1, binding = 2) uniform sampler2D u_NormalTexture;
layout(set = 1, binding = 3) uniform sampler2D u_MetallicRoughnessTexture;

/* After GBufferInstancedPush in gbuffer_instanced.vert (vec4 + 3 floats = 28 bytes). */
layout(push_constant, std430) uniform GBufferFragPush {
    layout(offset = 28) float u_HasAlbedoTexture;
} pc;

#include "../../common/material_surface.glsl"

void main() {
    g_Position = vec4(v_WorldPos, 1.0);
    bool useAlbedo = (pc.u_HasAlbedoTexture > 0.5);
    MaterialSurface surf;
    EvaluateMaterialSurface(
        v_TexCoord, v_TBN, v_WorldNormal,
        MaterialBlock.albedo, MaterialBlock.metallic, MaterialBlock.roughness, MaterialBlock.ao, MaterialBlock.emission, MaterialBlock.alpha, MaterialBlock.reflective,
        u_AlbedoTexture, u_NormalTexture, u_MetallicRoughnessTexture,
        u_MetallicRoughnessTexture, u_MetallicRoughnessTexture, u_MetallicRoughnessTexture, u_AlbedoTexture,
        useAlbedo, false, false, false, false, false, false,
        surf
    );
    g_Normal = vec4(surf.normal, 1.0);
    g_Albedo = vec4(surf.albedo, 1.0);
    g_PBRParams = vec3(surf.metallic, surf.roughness, surf.reflective);
    g_Velocity = v_Velocity;
}
