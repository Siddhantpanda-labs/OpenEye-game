#version 460

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_ViewPos;
layout(location = 2) in vec3 v_WorldNormal;
layout(location = 3) in vec2 v_TexCoord;
layout(location = 4) in mat3 v_TBN;

layout(location = 0) out vec4 FragColor;

#include "../../common/material_block.glsl"

layout(binding = 0) uniform sampler2D u_AlbedoTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;
layout(binding = 2) uniform sampler2D u_MetallicRoughnessTexture;
layout(binding = 5) buffer SceneLightingBlock {
    vec4 header0;
    vec4 header1;
    mat4 lightSpaceMatrix;
    vec4 shadowParams;
} sceneLight;

layout(binding = 6) uniform sampler2DArray u_ShadowMap;
layout(binding = 11) uniform samplerCube u_IrradianceMap;
layout(binding = 12) uniform samplerCube u_PrefilterMap;
layout(binding = 13) uniform sampler2D u_BRDFLUT;

layout(push_constant, std430) uniform ForwardPlusFragPush {
    int u_UseIBL;
} pc;

#include "../../common/material_surface.glsl"
#include "../../common/brdf.glsl"

void main() {
    MaterialSurface surf;
    EvaluateMaterialSurface(
        v_TexCoord, v_TBN, v_WorldNormal,
        MaterialBlock.albedo, MaterialBlock.metallic, MaterialBlock.roughness, MaterialBlock.ao, MaterialBlock.emission, MaterialBlock.alpha, MaterialBlock.reflective,
        u_AlbedoTexture, u_NormalTexture, u_MetallicRoughnessTexture,
        u_AlbedoTexture, u_AlbedoTexture, u_AlbedoTexture, u_AlbedoTexture,
        false, false, false, false, false, false, false,
        surf
    );
    vec3 V = normalize(v_ViewPos - v_WorldPos);
    vec3 L = normalize(sceneLight.header0.xyz);
    vec3 Lo = EvaluateBurleyBRDF(surf.albedo, surf.metallic, surf.roughness, surf.normal, V, L) * max(dot(surf.normal, L), 0.0);
    FragColor = vec4(surf.emission + Lo, surf.alpha);
}
