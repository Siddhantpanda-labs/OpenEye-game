#version 460

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_ViewPos;
layout(location = 2) in vec3 v_WorldNormal;
layout(location = 3) in vec2 v_TexCoord;
layout(location = 4) in mat3 v_TBN;

layout(location = 0) out vec4 FragColor;

// Set 2 = Material
layout(set = 2, binding = 0) uniform MaterialParams {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float alpha;
    vec3 emission;
    float reflective;
} MaterialBlock;

layout(set = 2, binding = 1) uniform sampler2D u_AlbedoTexture;

// Set 1 = ForwardPass
layout(set = 1, binding = 0) uniform sampler2DArray u_ShadowMap;
layout(set = 1, binding = 3) uniform samplerCube u_IrradianceMap;
layout(set = 1, binding = 4) uniform samplerCube u_PrefilterMap;
layout(set = 1, binding = 5) uniform sampler2D u_BRDFLUT;

// Set 4 = SceneBuffers (binding 2 = light buffer)
layout(set = 4, binding = 2) buffer SceneLightingBlock {
    vec4 header0;
    vec4 header1;
    mat4 lightSpaceMatrix;
} sceneLight;

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
        u_AlbedoTexture, u_AlbedoTexture, u_AlbedoTexture,
        u_AlbedoTexture, u_AlbedoTexture, u_AlbedoTexture, u_AlbedoTexture,
        false, false, false, false, false, false, false,
        surf
    );
    vec3 V = normalize(v_ViewPos - v_WorldPos);
    vec3 L = normalize(sceneLight.header0.xyz);
    vec3 Lo = EvaluateBurleyBRDF(surf.albedo, surf.metallic, surf.roughness, surf.normal, V, L) * max(dot(surf.normal, L), 0.0);
    FragColor = vec4(surf.emission + Lo, surf.alpha);
}
