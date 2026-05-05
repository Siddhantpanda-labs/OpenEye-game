// OpenEye next-gen rendering — M5 Deferred Lighting Analytic Fragment Shader
// Logical: raster/deferred/deferred_lighting_analytic
#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform PerViewUbo {
    mat4 uView;
    mat4 uProjection;
    mat4 uViewProjection;
    vec4 uViewPos;
    vec4 uViewport;
    vec4 uViewPad0;
    vec4 uViewPad1;
} uPerView;

layout(set = 1, binding = 0) uniform sampler2D tGBuffer0; // Albedo(RGB), Flags(A)
layout(set = 1, binding = 1) uniform sampler2D tGBuffer1; // Normal(XYZ), emissive strength (A)
layout(set = 1, binding = 2) uniform sampler2D tGBuffer2; // Metallic(R), Roughness(G), AO(B), Mask(A)
// Depth from the G-buffer render target (same image written by gbuffer_static), not Main HDR depth.
layout(set = 1, binding = 3) uniform sampler2D tDepth;

layout(std140, set = 1, binding = 4) uniform DirLightUbo {
    vec4 uLightDir;
    vec4 uLightColorIntensity;
    vec4 uDlPad[14];
} uDirLight;

// IBL split-sum (Karis / UE-style). Missing probes: engine binds neutral fallback cubemap + 2D LUT.
layout(set = 1, binding = 5) uniform samplerCube tIrradiance;
layout(set = 1, binding = 6) uniform samplerCube tPrefilter;
layout(set = 1, binding = 7) uniform sampler2D tBrdfLut;

const float PI = 3.14159265358979323846;
const float EPSILON = 1e-5;

// Function to reconstruct world position from depth
vec3 ReconstructWorldPos(vec2 uv, float depth, mat4 invViewProj) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = invViewProj * ndc;
    return world.xyz / world.w;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, EPSILON);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max((NdotV * (1.0 - k) + k), EPSILON);
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

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
        * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // Fullscreen-triangle VS: within the visible screen (clip [-1,1]), interpolated vUv is [0,1].
    const vec2 uv = clamp(vUv, vec2(0.0), vec2(0.999999));
    const ivec2 gbufSize = textureSize(tDepth, 0);
    const ivec2 pix = ivec2(uv * vec2(gbufSize));
    float depth = texelFetch(tDepth, pix, 0).r;

    vec4 gb0 = texelFetch(tGBuffer0, pix, 0);
    vec4 gb1 = texelFetch(tGBuffer1, pix, 0);

    if (depth >= 1.0) {
        // Skybox / background
        outColor = vec4(0.12, 0.14, 0.20, 1.0);
        return;
    }

    vec4 gb2 = texelFetch(tGBuffer2, pix, 0);

    vec3 albedo = gb0.rgb;
    vec3 N = normalize(gb1.xyz);
    float emissiveStrength = gb1.a;
    float metallic = gb2.r;
    float roughness = gb2.g;
    float ao = gb2.b;

    mat4 invViewProj = inverse(uPerView.uViewProjection);
    vec3 worldPos = ReconstructWorldPos(uv, depth, invViewProj);
    
    vec3 L = normalize(-uDirLight.uLightDir.xyz);
    vec3 V = normalize(uPerView.uViewPos.xyz - worldPos);
    
    float NdL = max(dot(N, L), 0.0);
    float NdV = max(dot(N, V), 0.0);
    vec3 H = normalize(V + L);
    float VdH = max(dot(V, H), 0.0);
    vec3 radiance = uDirLight.uLightColorIntensity.rgb * uDirLight.uLightColorIntensity.a;

    vec3 F0 = mix(vec3(0.04), albedo, clamp(metallic, 0.0, 1.0));
    vec3 F = FresnelSchlick(VdH, F0);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 numerator = NDF * G * F;
    float denominator = max(4.0 * NdV * NdL, EPSILON);
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    vec3 Fr = FresnelSchlickRoughness(NdV, F0, roughness);
    vec3 kS_IBL = Fr;
    vec3 kD_IBL = (vec3(1.0) - kS_IBL) * (1.0 - metallic);
    vec3 irradiance = texture(tIrradiance, N).rgb;
    vec3 iblDiffuse = irradiance * albedo * kD_IBL;

    vec3 R = reflect(-V, N);
    int prefilterLevels = textureQueryLevels(tPrefilter);
    float lod = roughness * float(max(prefilterLevels, 1) - 1);
    vec3 prefiltered = textureLod(tPrefilter, R, lod).rgb;
    vec2 envBrdf = texture(tBrdfLut, vec2(NdV, roughness)).rg;
    vec3 iblSpecular = prefiltered * (Fr * envBrdf.x + envBrdf.y);
    vec3 ibl = (iblDiffuse + iblSpecular) * ao;

    vec3 emissive = albedo * emissiveStrength;
    vec3 Lo = ibl + emissive + (diffuse + specular) * radiance * NdL;
    outColor = vec4(Lo, 1.0);
}
