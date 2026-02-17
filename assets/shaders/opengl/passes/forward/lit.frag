#version 450 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;

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
    float att = 1.0 - smoothstep(radius * 0.5, radius, dist);
    att *= att;
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
    float spot = smoothstep(outerCos, innerCos, cosTheta);
    float att = (1.0 - dist / range) * (1.0 - dist / range) * spot;
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
    vec3 V = normalize(u_ViewPos - v_Position);
    vec3 L = normalize(-u_LightDir);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = shadowFactor(v_Position);
    float lightVis = 1.0 - shadow;
    vec3 diffuse = u_Albedo * u_LightColor * u_LightIntensity * NdotL * lightVis;
    vec3 ambient = u_Albedo * u_Ambient;
    vec3 color = ambient + diffuse + u_Emission;

    float roughness = max(u_Roughness, 0.04);
    for (int i = 0; i < u_PointLightCount && i < MAX_POINT_LIGHTS; i++) {
        color += evalPointLight(v_Position, N, V, u_Albedo, u_Metallic, roughness,
            u_PointLightPos[i], u_PointLightColor[i], u_PointLightIntensity[i], u_PointLightRadius[i]);
    }
    for (int i = 0; i < u_SpotLightCount && i < MAX_SPOT_LIGHTS; i++) {
        color += evalSpotLight(v_Position, N, V, u_Albedo, u_Metallic, roughness,
            u_SpotLightPos[i], normalize(u_SpotLightDir[i]), u_SpotLightColor[i],
            u_SpotLightIntensity[i], u_SpotLightRange[i],
            u_SpotLightInnerCos[i], u_SpotLightOuterCos[i]);
    }

    color *= u_Exposure;
    FragColor = vec4(color, u_Alpha);
}
