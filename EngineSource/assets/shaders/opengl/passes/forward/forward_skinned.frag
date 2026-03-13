#version 460 core
@MATERIAL_DEFINES@

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;
in mat3 v_TBN;

uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_MetallicRoughnessTexture;
uniform sampler2D u_AOTexture;
uniform sampler2D u_EmissiveTexture;

uniform float u_HasNormalTexture;
uniform float u_HasMetallicRoughnessTexture;
uniform float u_HasAOTexture;
uniform float u_HasEmissiveTexture;

layout(std140, binding = 2) uniform MaterialBlock {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float alpha;
    vec3 emission;
    float reflective;
};

// Enhancement 6.1: scene lighting from SSBO (binding 5); shadow at 6
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
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
    ScenePointLightData pointLights[MAX_POINT_LIGHTS];
    SceneSpotLightData spotLights[MAX_SPOT_LIGHTS];
} sceneLight;

layout(binding = 6) uniform sampler2DArray u_ShadowMap;
layout(binding = 11) uniform samplerCube u_IrradianceMap;
layout(binding = 12) uniform samplerCube u_PrefilterMap;
layout(binding = 13) uniform sampler2D u_BRDFLUT;
uniform int u_UseIBL = 0;

out vec4 FragColor;

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

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 evalPointLight(vec3 pos, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
    vec3 lightPos, vec3 lightColor, float intensity, float radius) {
    vec3 L = lightPos - pos;
    float dist = length(L);
    if (dist >= radius) return vec3(0.0);
    L /= dist;
    float x = dist / max(radius, 1e-3);
    float rangeAtt = clamp(1.0 - x * x, 0.0, 1.0);
    rangeAtt *= rangeAtt;
    float invSq = 1.0 / (1.0 + dist * dist);
    float att = invSq * rangeAtt * 4.0;
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    vec3 diff = albedo * (1.0 - metallic) * lightColor * intensity * NdotL * att;
    diff += (0.04 + albedo * metallic) * lightColor * intensity * spec * att;
    return diff;
}

vec3 evalSpotLight(vec3 pos, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
    vec3 lightPos, vec3 lightDir, vec3 lightColor, float intensity, float range,
    float innerCos, float outerCos) {
    vec3 L = lightPos - pos;
    float dist = length(L);
    if (dist >= range) return vec3(0.0);
    L /= dist;
    float cosTheta = dot(-L, lightDir);
    float inner = max(innerCos, outerCos + 1e-4);
    float spot = clamp((cosTheta - outerCos) / (inner - outerCos), 0.0, 1.0);
    spot *= spot;
    float x = dist / max(range, 1e-3);
    float rangeAtt = clamp(1.0 - x * x, 0.0, 1.0);
    rangeAtt *= rangeAtt;
    float invSq = 1.0 / (1.0 + dist * dist);
    float att = invSq * rangeAtt * spot * 4.0;
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    vec3 diff = albedo * (1.0 - metallic) * lightColor * intensity * NdotL * att;
    diff += (0.04 + albedo * metallic) * lightColor * intensity * spec * att;
    return diff;
}

void main() {
    MaterialSurface surf;
    EvaluateMaterialSurface(
        v_TexCoord, v_TBN, v_Normal,
        MaterialBlock.albedo, MaterialBlock.metallic, MaterialBlock.roughness, MaterialBlock.ao, MaterialBlock.emission, MaterialBlock.alpha, MaterialBlock.reflective,
        u_AlbedoTexture, u_NormalTexture, u_MetallicRoughnessTexture,
        u_AlbedoTexture, u_AlbedoTexture, u_AOTexture, u_EmissiveTexture,
        true, u_HasNormalTexture > 0.5, u_HasMetallicRoughnessTexture > 0.5,
        false, false, u_HasAOTexture > 0.5, u_HasEmissiveTexture > 0.5,
        surf
    );
    vec3 N = surf.normal;
    vec3 albedoColor = surf.albedo;
    float metallic = surf.metallic;
    float roughness = max(surf.roughness * 0.65, 0.035);
    float ao = surf.ao;
    vec3 emission = surf.emission;

    vec3 V = normalize(sceneLight.header.viewPos - v_Position);
    vec3 L = normalize(-sceneLight.header.direction);
    vec3 H = normalize(V + L);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedoColor, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    specular *= 1.35;

    float NdotL = max(dot(N, L), 0.0);
    float shadow = shadowFactor(v_Position);
    float lightVis = 1.0 - shadow;
    vec3 Lo = (kD * albedoColor / PI + specular) * sceneLight.header.color * sceneLight.header.intensity * NdotL * lightVis;

    vec3 ambient = albedoColor * (1.0 - metallic) * sceneLight.header.ambient * ao;

    vec3 color = ambient + Lo;

    int numPoint = min(sceneLight.header.pointLightCount, MAX_POINT_LIGHTS);
    int numSpot = min(sceneLight.header.spotLightCount, MAX_SPOT_LIGHTS);
    for (int i = 0; i < numPoint; i++) {
        color += evalPointLight(v_Position, N, V, albedoColor, metallic, roughness,
            sceneLight.pointLights[i].position, sceneLight.pointLights[i].color,
            sceneLight.pointLights[i].intensity, sceneLight.pointLights[i].radius);
    }
    for (int i = 0; i < numSpot; i++) {
        color += evalSpotLight(v_Position, N, V, albedoColor, metallic, roughness,
            sceneLight.spotLights[i].position, normalize(sceneLight.spotLights[i].direction),
            sceneLight.spotLights[i].color, sceneLight.spotLights[i].intensity,
            sceneLight.spotLights[i].range,
            sceneLight.spotLights[i].innerConeCos, sceneLight.spotLights[i].outerConeCos);
    }

    if (u_UseIBL != 0) {
        float NdotV = max(dot(N, V), 0.0001);
        vec3 F0 = mix(vec3(0.04), albedoColor, metallic);
        vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kD = (1.0 - F) * (1.0 - metallic);
        vec3 irradiance = texture(u_IrradianceMap, N).rgb;
        color += kD * irradiance * albedoColor;
        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdfLUT = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
        color += prefilteredColor * (F * brdfLUT.x + brdfLUT.y);
    }

    color = (color + emission) * sceneLight.header.exposure;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(lum), color, 1.15);
    color = clamp(color, vec3(0.0), vec3(10.0));

    color = (color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14);
    color = pow(color, vec3(1.0/2.2));
    color = clamp(color, vec3(0.0), vec3(1.0));

    FragColor = vec4(color, 1.0);
}
