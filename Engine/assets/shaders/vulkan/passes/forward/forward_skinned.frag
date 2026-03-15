#version 460

layout(location = 0) in vec3 v_Position;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in mat3 v_TBN;

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
layout(set = 2, binding = 2) uniform sampler2D u_NormalTexture;
layout(set = 2, binding = 3) uniform sampler2D u_MetallicRoughnessTexture;

// Set 1 = ForwardPass (shadow, IBL)
layout(set = 1, binding = 0) uniform sampler2DArray u_ShadowMap;
layout(set = 1, binding = 3) uniform samplerCube u_IrradianceMap;
layout(set = 1, binding = 4) uniform samplerCube u_PrefilterMap;
layout(set = 1, binding = 5) uniform sampler2D u_BRDFLUT;

// Set 4 = SceneBuffers (binding 2 = light buffer)
layout(set = 4, binding = 2) buffer SceneLightingBlock {
    vec4 header0;
    vec4 header1;
    mat4 lightSpaceMatrix;
    vec4 shadowParams;
} sceneLight;

layout(push_constant, std430) uniform ForwardFragPush {
    int u_UseIBL;
    int u_UseAlbedoTexture;
} pc;

#include "../../common/material_surface.glsl"
#include "../../common/brdf.glsl"

void main() {
    MaterialSurface surf;
    EvaluateMaterialSurface(
        v_TexCoord, v_TBN, v_Normal,
        MaterialBlock.albedo, MaterialBlock.metallic, MaterialBlock.roughness, MaterialBlock.ao, MaterialBlock.emission, MaterialBlock.alpha, MaterialBlock.reflective,
        u_AlbedoTexture, u_NormalTexture, u_MetallicRoughnessTexture,
        u_MetallicRoughnessTexture, u_MetallicRoughnessTexture, u_MetallicRoughnessTexture, u_AlbedoTexture,
        pc.u_UseAlbedoTexture != 0, false, false, false, false, false, false,
        surf
    );
    vec3 V = normalize(-v_Position);
    vec3 L = normalize(sceneLight.header0.xyz);
    vec3 radiance = sceneLight.header1.xyz * sceneLight.header1.w;
    vec3 Lo = EvaluateBurleyBRDF(surf.albedo, surf.metallic, surf.roughness, surf.normal, V, L) * max(dot(surf.normal, L), 0.0);
    FragColor = vec4(surf.emission + Lo, surf.alpha);
}
