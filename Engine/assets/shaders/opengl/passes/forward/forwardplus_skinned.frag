#version 460 core
@MATERIAL_DEFINES@

in vec3 v_WorldPos;
in vec3 v_ViewPos;
in vec3 v_WorldNormal;
in vec2 v_TexCoord;
in mat3 v_TBN;

#include "../../common/material_block.glsl"
uniform sampler2D u_AlbedoTexture;
uniform int u_UseAlbedoTexture;
uniform sampler2D u_NormalTexture;
uniform int u_UseNormalTexture;
uniform sampler2D u_MetallicRoughnessTexture;
uniform int u_UseMetallicRoughnessTexture;
uniform sampler2D u_AOTexture;
uniform int u_UseAOTexture;
uniform sampler2D u_EmissiveTexture;
uniform int u_UseEmissiveTexture;
uniform int u_ScreenWidth;
uniform int u_ScreenHeight;
uniform int u_TileCountX;
uniform int u_TileCountY;
uniform int u_SliceCount;
uniform float u_LogNear;
uniform float u_LogFar;
uniform float u_NearPlane;
uniform float u_FarPlane;
uniform int u_PointLightCount;
uniform int u_SpotLightCount;
uniform sampler2DArray u_ShadowMap;
layout(binding = 11) uniform samplerCube u_IrradianceMap;
layout(binding = 12) uniform samplerCube u_PrefilterMap;
layout(binding = 13) uniform sampler2D u_BRDFLUT;
uniform int u_UseIBL = 0;

// Enhancement 6.1: scene lighting from SSBO (binding 5); point/spot read from 3/4 for tiled culling
#define SCENE_LIGHT_MAX_POINT 8
#define SCENE_LIGHT_MAX_SPOT 4
struct SceneLightHeader {
    vec3 direction; float pad0;
    vec3 color; float pad1;
    float intensity; float pad2; float pad3; float pad4;
    vec3 ambient; float pad5;
    float exposure; float pad6; float pad7; float pad8;
    vec3 viewPos; float pad9;
    mat4 lightSpaceMatrix;
    float shadowBias; float pad10; float pad11; float pad12;
    int useShadows; float pad13; float pad14; float pad15;
    int pointLightCount; int spotLightCount; float pad16; float pad17;
    int cascadeCount; float pad18; float pad19; float pad20;
    vec4 farBounds;
};
struct ScenePointLightData {
    vec3 position; float pad0;
    vec3 color; float intensity;
    float radius; float pad1; float pad2; float pad3;
};
struct SceneSpotLightData {
    vec3 position; float pad0;
    vec3 direction; float pad1;
    vec3 color; float intensity;
    float range; float innerConeCos; float outerConeCos;
    float pad2; float pad3; float pad4; float pad5; float pad6;
};
layout(std140, binding = 5) buffer SceneLightingBlock {
    SceneLightHeader header;
    ScenePointLightData pointLights[SCENE_LIGHT_MAX_POINT];
    SceneSpotLightData spotLights[SCENE_LIGHT_MAX_SPOT];
} sceneLight;

layout(binding = 2) restrict readonly buffer LightIndexBuffer { uint lightIndices[]; };

struct PointLight {
    vec3 position;
    float _p0;
    vec3 color;
    float intensity;
    float radius;
    float _p1;
    float _p2;
    float _p3;
};
struct SpotLight {
    vec3 position;
    float _p0;
    vec3 direction;
    float _p1;
    vec3 color;
    float intensity;
    float range;
    float innerConeCos;
    float outerConeCos;
    float _p2;
    float _p3;
};
layout(binding = 3) restrict readonly buffer PointLightBuffer { PointLight pointLights[]; };
layout(binding = 4) restrict readonly buffer SpotLightBuffer { SpotLight spotLights[]; };

layout(location = 0) out vec4 o_Color;

#include "../../common/material_surface.glsl"
#include "../../common/brdf.glsl"

float shadowFactor(vec3 worldPos) {
    if (sceneLight.header.useShadows <= 0) return 0.0;
    vec4 lightSpacePos = sceneLight.header.lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
    ndc = ndc * 0.5 + 0.5;
    if (ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0)
        return 0.0;
    float depth = texture(u_ShadowMap, vec3(ndc.xy, 0.0)).r;
    return (ndc.z - sceneLight.header.shadowBias > depth) ? 1.0 : 0.0;
}

vec3 calc_point_light(vec3 pos, vec3 color, float intensity, float radius,
    vec3 fragPos, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float roughness) {
    vec3 toLight = pos - fragPos;
    float dist = length(toLight);
    if (dist >= radius) return vec3(0.0);
    vec3 lightDir = toLight / max(dist, 1e-5);
    float x = dist / max(radius, 1e-3);
    float rangeAtt = clamp(1.0 - x * x, 0.0, 1.0);
    rangeAtt *= rangeAtt;
    float invSq = 1.0 / (1.0 + dist * dist);
    float atten = invSq * rangeAtt * 4.0;
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 H = normalize(viewDir + lightDir);
    float NdotH = max(dot(normal, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    vec3 diff = albedo * (1.0 - metallic) * NdotL + (0.04 + albedo * metallic) * spec;
    return diff * color * intensity * atten;
}

vec3 calc_spot_light(vec3 pos, vec3 dir, vec3 color, float intensity, float range,
    float innerCos, float outerCos, vec3 fragPos, vec3 normal, vec3 viewDir,
    vec3 albedo, float metallic, float roughness) {
    vec3 toLight = pos - fragPos;
    float dist = length(toLight);
    if (dist > range) return vec3(0.0);
    vec3 lightDir = toLight / max(dist, 1e-5);
    float cosAngle = dot(lightDir, -dir);
    float inner = max(innerCos, outerCos + 1e-4);
    float spot = clamp((cosAngle - outerCos) / (inner - outerCos), 0.0, 1.0);
    spot *= spot;
    float x = dist / max(range, 1e-3);
    float rangeAtt = clamp(1.0 - x * x, 0.0, 1.0);
    rangeAtt *= rangeAtt;
    float invSq = 1.0 / (1.0 + dist * dist);
    float atten = invSq * rangeAtt * 4.0;
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 H = normalize(viewDir + lightDir);
    float NdotH = max(dot(normal, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    vec3 diff = albedo * (1.0 - metallic) * NdotL + (0.04 + albedo * metallic) * spec;
    return diff * color * intensity * atten * spot;
}

void main() {
    MaterialSurface surf;
    EvaluateMaterialSurface(
        v_TexCoord, v_TBN, v_WorldNormal,
        MaterialBlock.albedo, MaterialBlock.metallic, MaterialBlock.roughness, MaterialBlock.ao, MaterialBlock.emission, MaterialBlock.alpha, MaterialBlock.reflective,
        u_AlbedoTexture, u_NormalTexture, u_MetallicRoughnessTexture,
        u_AlbedoTexture, u_AlbedoTexture, u_AOTexture, u_EmissiveTexture,
        u_UseAlbedoTexture > 0, u_UseNormalTexture > 0, u_UseMetallicRoughnessTexture > 0,
        false, false, u_UseAOTexture > 0, u_UseEmissiveTexture > 0,
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
    float shadow = shadowFactor(v_WorldPos);
    float lightVis = 1.0 - shadow;
    result += albedo * sceneLight.header.color * sceneLight.header.intensity * NdotL * lightVis;

    float viewZ = abs(v_ViewPos.z);
    float viewZClamped = clamp(viewZ, u_NearPlane, u_FarPlane);
    float logViewZ = log(viewZClamped);
    float sliceF = (logViewZ - u_LogNear) / (u_LogFar - u_LogNear) * float(u_SliceCount);
    uint sliceZ = uint(clamp(sliceF, 0.0, float(u_SliceCount) - 1.0));
    uint tileX = uint(gl_FragCoord.x) / 16u;
    uint tileY = uint(gl_FragCoord.y) / 16u;
    uint clusterIdx = (sliceZ * uint(u_TileCountY) + tileY) * uint(u_TileCountX) + tileX;
    uint totalClusters = uint(u_TileCountX) * uint(u_TileCountY) * uint(u_SliceCount);
    uint baseHeaders = 1u;
    uint clusterOffset = lightIndices[baseHeaders + clusterIdx * 2u];
    uint clusterCount = lightIndices[baseHeaders + clusterIdx * 2u + 1u];
    uint baseIndices = baseHeaders + totalClusters * 2u;

    for (uint i = 0u; i < clusterCount; i++) {
        uint lidx = lightIndices[baseIndices + clusterOffset + i];
        int idx = int(lidx);
        if (idx < u_PointLightCount) {
            result += calc_point_light(
                pointLights[idx].position, pointLights[idx].color,
                pointLights[idx].intensity, pointLights[idx].radius,
                v_WorldPos, N, V, albedo, metallic, roughness);
        } else {
            int sidx = idx - u_PointLightCount;
            if (sidx < u_SpotLightCount) {
                result += calc_spot_light(
                    spotLights[sidx].position, spotLights[sidx].direction,
                    spotLights[sidx].color, spotLights[sidx].intensity, spotLights[sidx].range,
                    spotLights[sidx].innerConeCos, spotLights[sidx].outerConeCos,
                    v_WorldPos, N, V, albedo, metallic, roughness);
            }
        }
    }

    if (u_UseIBL != 0) {
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
    o_Color = vec4(result * sceneLight.header.exposure, MaterialBlock.alpha);
}
