#version 430 core

in vec3 v_WorldPos;
in vec3 v_ViewPos;
in vec3 v_WorldNormal;
in vec2 v_TexCoord;

uniform vec3 u_ViewPos;
uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_DirLightDir;
uniform vec3 u_DirLightColor;
uniform float u_DirLightIntensity;
uniform vec3 u_Ambient;
uniform float u_Exposure;
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

vec3 calc_point_light(vec3 pos, vec3 color, float intensity, float radius,
    vec3 fragPos, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float roughness) {
    vec3 lightDir = normalize(pos - fragPos);
    float dist = length(pos - fragPos);
    float atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
    atten *= smoothstep(radius, radius * 0.5, dist);
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
    vec3 lightDir = normalize(pos - fragPos);
    float dist = length(pos - fragPos);
    if (dist > range) return vec3(0.0);
    float cosAngle = dot(lightDir, -dir);
    float spot = smoothstep(outerCos, innerCos, cosAngle);
    float atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 H = normalize(viewDir + lightDir);
    float NdotH = max(dot(normal, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    vec3 diff = albedo * (1.0 - metallic) * NdotL + (0.04 + albedo * metallic) * spec;
    return diff * color * intensity * atten * spot;
}

void main() {
    vec3 N = normalize(v_WorldNormal);
    vec3 V = normalize(u_ViewPos - v_WorldPos);
    vec3 albedo = u_Albedo;
    float metallic = u_Metallic;
    float roughness = max(u_Roughness, 0.04);

    vec3 result = albedo * u_Ambient;
    vec3 L = normalize(-u_DirLightDir);
    float NdotL = max(dot(N, L), 0.0);
    result += albedo * u_DirLightColor * u_DirLightIntensity * NdotL;

    float viewZ = abs(v_ViewPos.z);
    float viewZClamped = clamp(viewZ, u_NearPlane, u_FarPlane);
    float logViewZ = log(viewZClamped);
    float sliceF = (logViewZ - u_LogNear) / (u_LogFar - u_LogNear) * float(u_SliceCount);
    uint sliceZ = uint(clamp(sliceF, 0.0, float(u_SliceCount) - 1.0));
    uint tileX = uint(gl_FragCoord.x) / 16u;
    uint tileY = uint(gl_FragCoord.y) / 16u;
    uint clusterIdx = (sliceZ * uint(u_TileCountY) + tileY) * uint(u_TileCountX) + tileX;
    uint offset = clusterIdx * 255u;

    for (uint i = 0u; i < 255u; i++) {
        uint lidx = lightIndices[offset + i];
        if (lidx >= 255u) break;
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

    o_Color = vec4(result * u_Exposure, 1.0);
}
