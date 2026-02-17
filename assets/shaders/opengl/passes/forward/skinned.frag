#version 450 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_MetallicRoughnessTexture;
uniform sampler2D u_AOTexture;
uniform sampler2D u_EmissiveTexture;

uniform float u_HasNormalTexture;
uniform float u_HasMetallicRoughnessTexture;
uniform float u_HasAOTexture;
uniform float u_HasEmissiveTexture;

uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_Emission;
uniform float u_AO;

uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform vec3 u_ViewPos;
uniform float u_LightIntensity;
uniform vec3 u_Ambient = vec3(0.05);
uniform float u_Exposure = 1.2;

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

layout(binding = 5) uniform sampler2D u_ShadowMap;
uniform mat4 u_LightSpaceMatrix;
uniform float u_ShadowBias = 0.005;
uniform int u_UseShadows = 0;

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

const float PI = 3.14159265359;

// Normal Distribution Function (GGX/Trowbridge-Reitz)
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

// Geometry Function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

// Geometry Function (Smith)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
    float att = (1.0 - dist / range);
    att = att * att * spot;
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
    
    // Normal mapping (simplified - assumes normal map is in world space or object space)
    // TODO: Add proper TBN matrix support in vertex shader for tangent-space normal maps
    if (u_HasNormalTexture > 0.5) {
        vec3 normalSample = texture(u_NormalTexture, v_TexCoord).rgb;
        normalSample = normalSample * 2.0 - 1.0; // Convert from [0,1] to [-1,1]
        // Simple blend with vertex normal (proper implementation needs TBN)
        N = normalize(mix(N, normalSample, 0.5));
    }
    
    vec3 V = normalize(u_ViewPos - v_Position);
    vec3 L = normalize(-u_LightDir);
    vec3 H = normalize(V + L);
    
    // Sample textures
    vec3 albedoColor = texture(u_AlbedoTexture, v_TexCoord).rgb * u_Albedo;
    
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_HasMetallicRoughnessTexture > 0.5) {
        vec2 mr = texture(u_MetallicRoughnessTexture, v_TexCoord).bg; // glTF: B=metallic, G=roughness
        metallic = mix(metallic, mr.x, 1.0);
        roughness = mix(roughness, mr.y, 1.0);
    }
    // Shinier: lower effective roughness for stronger specular
    roughness = max(roughness * 0.65, 0.035);
    
    float ao = u_AO;
    if (u_HasAOTexture > 0.5) {
        ao = texture(u_AOTexture, v_TexCoord).r;
    }
    
    vec3 emission = u_Emission;
    if (u_HasEmissiveTexture > 0.5) {
        emission = texture(u_EmissiveTexture, v_TexCoord).rgb;
    }
    
    // Calculate F0 for dielectrics, blend with albedo for metals
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedoColor, metallic);
    
    // Cook-Torrance BRDF
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
    vec3 Lo = (kD * albedoColor / PI + specular) * u_LightColor * u_LightIntensity * NdotL * lightVis;

    // Ambient lighting
    vec3 ambient = u_Ambient * albedoColor * ao;

    vec3 color = ambient + Lo;

    for (int i = 0; i < u_PointLightCount && i < MAX_POINT_LIGHTS; i++) {
        color += evalPointLight(v_Position, N, V, albedoColor, metallic, roughness,
            u_PointLightPos[i], u_PointLightColor[i], u_PointLightIntensity[i], u_PointLightRadius[i]);
    }
    for (int i = 0; i < u_SpotLightCount && i < MAX_SPOT_LIGHTS; i++) {
        color += evalSpotLight(v_Position, N, V, albedoColor, metallic, roughness,
            u_SpotLightPos[i], normalize(u_SpotLightDir[i]), u_SpotLightColor[i],
            u_SpotLightIntensity[i], u_SpotLightRange[i],
            u_SpotLightInnerCos[i], u_SpotLightOuterCos[i]);
    }

    color = (color + emission) * u_Exposure;
    
    // HDR tonemapping (ACES fitted)
    color = (color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14);
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    // Vibrancy: stronger saturation boost so colors pop
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(lum), color, 1.3);
    color = clamp(color, vec3(0.0), vec3(1.0));
    
    FragColor = vec4(color, 1.0);
}
