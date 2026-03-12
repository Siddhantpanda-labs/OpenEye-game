#version 460 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;

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
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform vec3 u_ViewPos;
uniform float u_LightIntensity = 1.0;
uniform vec3 u_Ambient = vec3(0.1);
uniform float u_Exposure = 1.0;
uniform float u_Alpha = 1.0;

layout(binding = 5) uniform sampler2D u_ShadowMap;
uniform mat4 u_LightSpaceMatrix;
uniform float u_ShadowBias = 0.005;
uniform int u_UseShadows = 0;

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
uniform int u_PointLightCount;
uniform vec3 u_PointLightPos[MAX_POINT_LIGHTS];
uniform vec3 u_PointLightColor[MAX_POINT_LIGHTS];
uniform float u_PointLightIntensity[MAX_POINT_LIGHTS];
uniform float u_PointLightRadius[MAX_POINT_LIGHTS];

uniform int u_SpotLightCount;
uniform vec3 u_SpotLightPos[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightDir[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightColor[MAX_SPOT_LIGHTS];
uniform float u_SpotLightIntensity[MAX_SPOT_LIGHTS];
uniform float u_SpotLightRange[MAX_SPOT_LIGHTS];
uniform float u_SpotLightInnerCos[MAX_SPOT_LIGHTS];
uniform float u_SpotLightOuterCos[MAX_SPOT_LIGHTS];

out vec4 FragColor;

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
        // Forward lit path currently has no TBN in this shader; blend as graceful fallback.
        vec3 normalSample = texture(u_NormalTexture, v_TexCoord).rgb * 2.0 - 1.0;
        N = normalize(mix(N, normalSample, 0.5));
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
        vec2 mr = texture(u_MetallicRoughnessTexture, v_TexCoord).bg; // glTF: B=metallic, G=roughness
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

    vec3 V = normalize(u_ViewPos - v_Position);
    vec3 L = normalize(-u_LightDir);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = shadowFactor(v_Position);
    float lightVis = 1.0 - shadow;
    vec3 diffuse = albedo * u_LightColor * u_LightIntensity * NdotL * lightVis;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 ambient = (albedo * (1.0 - metallic) + F0 * metallic) * u_Ambient * ao;
    vec3 color = ambient + diffuse + emission;

    for (int i = 0; i < u_PointLightCount && i < MAX_POINT_LIGHTS; i++) {
        color += evalPointLight(v_Position, N, V, albedo, metallic, roughness,
            u_PointLightPos[i], u_PointLightColor[i], u_PointLightIntensity[i], u_PointLightRadius[i]);
    }
    for (int i = 0; i < u_SpotLightCount && i < MAX_SPOT_LIGHTS; i++) {
        color += evalSpotLight(v_Position, N, V, albedo, metallic, roughness,
            u_SpotLightPos[i], normalize(u_SpotLightDir[i]), u_SpotLightColor[i],
            u_SpotLightIntensity[i], u_SpotLightRange[i],
            u_SpotLightInnerCos[i], u_SpotLightOuterCos[i]);
    }

    color *= u_Exposure;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(lum), color, 1.15);
    color = clamp(color, vec3(0.0), vec3(10.0));
    FragColor = vec4(color, alpha);
}
