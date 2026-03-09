#version 460 core

in vec3 v_WorldPos;
in vec3 v_ViewPos;
in vec3 v_WorldNormal;
in vec2 v_TexCoord;
in mat3 v_TBN;
flat in vec3 v_CameraWorldPos;

uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
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
uniform vec3 u_Emission;
uniform float u_Alpha;
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
uniform vec3 u_PointLightPos[8];
uniform vec3 u_PointLightColor[8];
uniform float u_PointLightIntensity[8];
uniform float u_PointLightRadius[8];
uniform vec3 u_SpotLightPos[4];
uniform vec3 u_SpotLightDir[4];
uniform vec3 u_SpotLightColor[4];
uniform float u_SpotLightIntensity[4];
uniform float u_SpotLightRange[4];
uniform float u_SpotLightInnerCos[4];
uniform float u_SpotLightOuterCos[4];
uniform sampler2D u_ShadowMap;
uniform mat4 u_LightSpaceMatrix;
uniform float u_ShadowBias;
uniform int u_UseShadows;

layout(location = 0) out vec4 o_Color;

float shadowFactor(vec3 worldPos) {
    if (u_UseShadows <= 0) return 0.0;
    vec4 lightSpacePos = u_LightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
    ndc = ndc * 0.5 + 0.5;
    if (ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0)
        return 0.0;
    float depth = texture(u_ShadowMap, ndc.xy).r;
    return (ndc.z - u_ShadowBias > depth) ? 1.0 : 0.0;
}

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
    if (u_UseNormalTexture > 0) {
        vec3 n = texture(u_NormalTexture, v_TexCoord).xyz * 2.0 - 1.0;
        N = normalize(v_TBN * n);
    }
    vec3 V = normalize(v_CameraWorldPos - v_WorldPos);
    vec3 albedo = u_UseAlbedoTexture > 0
        ? texture(u_AlbedoTexture, v_TexCoord).rgb * u_Albedo
        : u_Albedo;
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_UseMetallicRoughnessTexture > 0) {
        vec2 mr = texture(u_MetallicRoughnessTexture, v_TexCoord).bg;
        metallic = mr.x;
        roughness = mr.y;
    }
    roughness = max(roughness, 0.04);

    float ao = u_AO;
    if (u_UseAOTexture > 0) {
        ao = texture(u_AOTexture, v_TexCoord).r;
    }

    vec3 emission = u_Emission;
    if (u_UseEmissiveTexture > 0) {
        emission = texture(u_EmissiveTexture, v_TexCoord).rgb;
    }

    vec3 result = albedo * u_Ambient * ao;
    vec3 L = normalize(-u_DirLightDir);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = shadowFactor(v_WorldPos);
    float lightVis = 1.0 - shadow;
    result += albedo * u_DirLightColor * u_DirLightIntensity * NdotL * lightVis;

    for (int i = 0; i < u_PointLightCount && i < 8; i++) {
        result += calc_point_light(
            u_PointLightPos[i], u_PointLightColor[i],
            u_PointLightIntensity[i], u_PointLightRadius[i],
            v_WorldPos, N, V, albedo, metallic, roughness);
    }
    for (int i = 0; i < u_SpotLightCount && i < 4; i++) {
        result += calc_spot_light(
            u_SpotLightPos[i], u_SpotLightDir[i],
            u_SpotLightColor[i], u_SpotLightIntensity[i], u_SpotLightRange[i],
            u_SpotLightInnerCos[i], u_SpotLightOuterCos[i],
            v_WorldPos, N, V, albedo, metallic, roughness);
    }

    result += emission;
    o_Color = vec4(result * u_Exposure, u_Alpha);
}
