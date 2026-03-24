#version 460
// Resource-space: set 1 ForwardPass, set 2 Material, set 4 SceneBuffers. Same as ForwardPlus.

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_ViewPos;
layout(location = 2) in vec3 v_WorldNormal;
layout(location = 3) in vec2 v_TexCoord;
layout(location = 4) in mat3 v_TBN;

layout(location = 0) out vec4 FragColor;

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

layout(set = 1, binding = 0) uniform sampler2DArray u_ShadowMap;
layout(set = 1, binding = 3) uniform samplerCube u_IrradianceMap;
layout(set = 1, binding = 4) uniform samplerCube u_PrefilterMap;
layout(set = 1, binding = 5) uniform sampler2D u_BRDFLUT;

#include "../../common/scene_lighting_types.glsl"

layout(set = 4, binding = 2, std140) buffer SceneLightingBlock {
    SceneLightHeader header;
    PointLightData pointLights[MAX_POINT_LIGHTS];
    SpotLightData spotLights[MAX_SPOT_LIGHTS];
} sceneLight;

layout(push_constant, std430) uniform ForwardPlusFragPush {
    int u_UseIBL;
} pc;

#include "../../common/forward_lighting_common.glsl"
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
    vec3 N = surf.normal;
    vec3 albedo = surf.albedo;
    float metallic = surf.metallic;
    float roughness = surf.roughness;
    float ao = surf.ao;
    vec3 emission = surf.emission;

    vec3 V = normalize(sceneLight.header.viewPos - v_WorldPos);
    vec3 result = albedo * sceneLight.header.ambient * ao;
    vec3 L = normalize(-sceneLight.header.direction);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = ForwardShadowFactor(v_WorldPos);
    float lightVis = 1.0 - shadow;
    result += albedo * sceneLight.header.color * sceneLight.header.intensity * NdotL * lightVis;

    int numPoint = min(sceneLight.header.pointLightCount, MAX_POINT_LIGHTS);
    int numSpot = min(sceneLight.header.spotLightCount, MAX_SPOT_LIGHTS);
    for (int i = 0; i < numPoint; i++) {
        result += ForwardEvalPointLight(v_WorldPos, N, V, albedo, metallic, roughness,
            sceneLight.pointLights[i].position, sceneLight.pointLights[i].color,
            sceneLight.pointLights[i].intensity, sceneLight.pointLights[i].radius);
    }
    for (int i = 0; i < numSpot; i++) {
        result += ForwardEvalSpotLight(v_WorldPos, N, V, albedo, metallic, roughness,
            sceneLight.spotLights[i].position, normalize(sceneLight.spotLights[i].direction),
            sceneLight.spotLights[i].color, sceneLight.spotLights[i].intensity,
            sceneLight.spotLights[i].range,
            sceneLight.spotLights[i].innerConeCos, sceneLight.spotLights[i].outerConeCos);
    }

    if (pc.u_UseIBL != 0) {
        float NdotV = max(dot(N, V), 0.0001);
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kD = (1.0 - F) * (1.0 - metallic);
        vec3 irradiance = texture(u_IrradianceMap, N).rgb;
        result += kD * irradiance * albedo;
        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdfLUT = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
        result += prefilteredColor * (F * brdfLUT.x + brdfLUT.y);
    }

    result += emission;
    FragColor = vec4(result * sceneLight.header.exposure, MaterialBlock.alpha);
}
