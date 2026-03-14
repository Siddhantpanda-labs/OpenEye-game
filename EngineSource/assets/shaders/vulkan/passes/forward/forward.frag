#version 460

layout(location = 0) in vec3 v_Position;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in mat3 v_TBN;

layout(location = 0) out vec4 FragColor;

#include "../../common/material_block.glsl"

layout(binding = 0) uniform sampler2D u_AlbedoTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;
layout(binding = 2) uniform sampler2D u_MetallicRoughnessTexture;
layout(binding = 3) uniform sampler2D u_MetallicTexture;
layout(binding = 3) uniform sampler2D u_RoughnessTexture;
layout(binding = 3) uniform sampler2D u_AOTexture;
layout(binding = 3) uniform sampler2D u_EmissiveTexture;

layout(binding = 5) buffer SceneLightingBlock {
    vec4 header0;
    vec4 header1;
    vec4 header2;
    mat4 lightSpaceMatrix;
    vec4 shadowParams;
    vec4 farBounds;
    vec4 pointLights[8];
    vec4 spotLights[4];
} sceneLight;

layout(binding = 6) uniform sampler2DArray u_ShadowMap;
layout(binding = 11) uniform samplerCube u_IrradianceMap;
layout(binding = 12) uniform samplerCube u_PrefilterMap;
layout(binding = 13) uniform sampler2D u_BRDFLUT;

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
        u_MetallicTexture, u_RoughnessTexture, u_AOTexture, u_EmissiveTexture,
        pc.u_UseAlbedoTexture != 0, false, false, false, false, false, false,
        surf
    );
    vec3 V = normalize(-v_Position);
    vec3 L = normalize(sceneLight.header0.xyz);
    vec3 radiance = sceneLight.header1.xyz * sceneLight.header1.w;
    vec3 Lo = EvaluateBurleyBRDF(surf.albedo, surf.metallic, surf.roughness, surf.normal, V, L) * max(dot(surf.normal, L), 0.0);
    FragColor = vec4(surf.emission + Lo, surf.alpha);
}
