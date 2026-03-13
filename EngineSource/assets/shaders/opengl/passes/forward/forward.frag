#version 460 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;
in mat3 v_TBN;

uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_MetallicRoughnessTexture;
uniform sampler2D u_MetallicTexture;
uniform sampler2D u_RoughnessTexture;
uniform sampler2D u_AOTexture;
uniform sampler2D u_EmissiveTexture;

uniform int u_UseAlbedoTexture = 0;
uniform int u_UseNormalTexture = 0;
uniform int u_UseMetallicRoughnessTexture = 0;
uniform int u_UseMetallicTexture = 0;
uniform int u_UseRoughnessTexture = 0;
uniform int u_UseAOTexture = 0;
uniform int u_UseEmissiveTexture = 0;

uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_Emission;
uniform float u_Alpha = 1.0;

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

out vec4 FragColor;

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
    vec3 N = normalize(v_Normal);
    if (u_UseNormalTexture > 0) {
        vec3 normalSample = texture(u_NormalTexture, v_TexCoord).rgb * 2.0 - 1.0;
        N = normalize(v_TBN * normalSample);
    }

    vec3 albedo = u_Albedo;
    float alpha = u_Alpha;
    if (u_UseAlbedoTexture > 0) {
        vec4 texel = texture(u_AlbedoTexture, v_TexCoord);
        albedo *= texel.rgb;
        alpha *= texel.a;
    }

    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_UseMetallicRoughnessTexture > 0) {
        vec2 mr = texture(u_MetallicRoughnessTexture, v_TexCoord).bg;
        metallic *= mr.x;
        roughness *= mr.y;
    } else {
        if (u_UseMetallicTexture > 0) metallic *= texture(u_MetallicTexture, v_TexCoord).r;
        if (u_UseRoughnessTexture > 0) roughness *= texture(u_RoughnessTexture, v_TexCoord).r;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    float ao = 1.0;
    if (u_UseAOTexture > 0) ao = texture(u_AOTexture, v_TexCoord).r;

    vec3 emission = u_Emission;
    if (u_UseEmissiveTexture > 0) emission += texture(u_EmissiveTexture, v_TexCoord).rgb;

    vec3 V = normalize(sceneLight.header.viewPos - v_Position);
    vec3 L = normalize(-sceneLight.header.direction);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = shadowFactor(v_Position);
    float lightVis = 1.0 - shadow;
    vec3 diffuse = albedo * sceneLight.header.color * sceneLight.header.intensity * NdotL * lightVis;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 ambient = (albedo * (1.0 - metallic) + F0 * metallic) * sceneLight.header.ambient * ao;
    vec3 color = ambient + diffuse + emission;

    int numPoint = min(sceneLight.header.pointLightCount, MAX_POINT_LIGHTS);
    int numSpot = min(sceneLight.header.spotLightCount, MAX_SPOT_LIGHTS);
    for (int i = 0; i < numPoint; i++) {
        color += evalPointLight(v_Position, N, V, albedo, metallic, roughness,
            sceneLight.pointLights[i].position, sceneLight.pointLights[i].color,
            sceneLight.pointLights[i].intensity, sceneLight.pointLights[i].radius);
    }
    for (int i = 0; i < numSpot; i++) {
        color += evalSpotLight(v_Position, N, V, albedo, metallic, roughness,
            sceneLight.spotLights[i].position, normalize(sceneLight.spotLights[i].direction),
            sceneLight.spotLights[i].color, sceneLight.spotLights[i].intensity,
            sceneLight.spotLights[i].range,
            sceneLight.spotLights[i].innerConeCos, sceneLight.spotLights[i].outerConeCos);
    }

    color *= sceneLight.header.exposure;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(lum), color, 1.15);
    color = clamp(color, vec3(0.0), vec3(10.0));
    FragColor = vec4(color, alpha);
}
