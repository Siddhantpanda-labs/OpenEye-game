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
layout(std140, set = 1, binding = 8) uniform ShadowParamsUbo {
    vec4 uCascadeSplits;      // xyz used for split end distances, w reserved.
    vec4 uShadowMeta;         // x=depthBias, y=normalBias, z=reserved, w=cascadeBlend
    vec4 uFilterExtent;       // xyz used for filter extents of cascades 0, 1, 2.
    mat4 uLightViewProj[3];
} uShadow;
layout(set = 1, binding = 9) uniform sampler2DArray tDirectionalShadowMap;

struct SpotLight {
    vec4 positionAndRange;      // xyz = position, w = range
    vec4 directionAndInnerCone; // xyz = direction, w = innerConeCos
    vec4 colorAndOuterCone;     // xyz = color * intensity, w = outerConeCos
    mat4 lightViewProj;
    vec4 shadowParams;          // x = shadowMapIndex (-1 if none), yzw = padding
};

layout(std140, set = 1, binding = 10) uniform SpotLightUbo {
    vec4 countAndPadding; // x = count
    SpotLight lights[16];
} uSpotLights;

layout(set = 1, binding = 11) uniform sampler2DArray tSpotShadowMap;

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

float linstep(float low, float high, float v) {
    return clamp((v - low) / (high - low), 0.0, 1.0);
}

float ComputeVsmVisibility(vec2 moments, float compareDepth, float minVariance, float bleedLow, float bleedHigh) {
    float p = step(compareDepth, moments.x);
    float variance = max(moments.y - moments.x * moments.x, minVariance);
    float d = compareDepth - moments.x;
    float pMax = linstep(bleedLow, bleedHigh, variance / (variance + d * d));
    return min(max(p, pMax), 1.0);
}

float ComputeStableReceiverDepth(float shadowDepth, vec3 N, vec3 L, float baseBias, float slopeScale, float derivScale) {
    float slope = max(1.0 - dot(N, L), 0.0);
    float slopeBias = baseBias * slope * slopeScale;
    float derivativeBias = fwidth(shadowDepth) * derivScale;
    float receiverBias = baseBias + slopeBias + derivativeBias;
    return shadowDepth - receiverBias;
}

float SampleDepthOnlyPcf2DArray(sampler2DArray shadowMap, vec2 uv, int layer, float compareDepth, int radius) {
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);
    float lit = 0.0;
    float taps = 0.0;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            vec2 pcfUv = uv + vec2(float(x), float(y)) * texelSize;
            float mapDepth = texture(shadowMap, vec3(pcfUv, float(layer))).r;
            lit += (compareDepth <= mapDepth) ? 1.0 : 0.0;
            taps += 1.0;
        }
    }
    return lit / max(taps, 1.0);
}

float SampleDirectionalShadowCascadeVSM(vec3 worldPos, vec3 N, vec3 L, int cascadeIndex) {
    // T1.1 Normal Offset Bias
    float normalBias = max(uShadow.uShadowMeta.y, 0.0) * max(1.0 - dot(N, L), 0.1);
    vec3 biasedPos = worldPos + N * normalBias;
    vec4 lightClip = uShadow.uLightViewProj[cascadeIndex] * vec4(biasedPos, 1.0);
    if (lightClip.w <= 0.0) {
        return 1.0;
    }
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec2 shadowUv = lightNdc.xy * 0.5 + 0.5;
    float shadowDepth = ComputeStableReceiverDepth(lightNdc.z, N, L, max(uShadow.uShadowMeta.x, 0.00005), 1.0, 2.0);
    if (shadowUv.x <= 0.0 || shadowUv.x >= 1.0 || shadowUv.y <= 0.0 || shadowUv.y >= 1.0 || shadowDepth <= 0.0 || shadowDepth >= 1.0) {
        return 1.0;
    }

    vec2 moments = texture(tDirectionalShadowMap, vec3(shadowUv, float(cascadeIndex))).rg;
    float vsm = ComputeVsmVisibility(moments, shadowDepth, 0.00008, 0.40, 1.0);
    float pcf = SampleDepthOnlyPcf2DArray(tDirectionalShadowMap, shadowUv, cascadeIndex, shadowDepth, 1);
    float contactWeight = 1.0 - smoothstep(0.002, 0.02, abs(shadowDepth - moments.x));
    // Use PCF clamp only near contact to keep dark cores without introducing lit-side striping.
    return mix(vsm, min(vsm, pcf), contactWeight);
}

float SampleDirectionalShadowVSM(vec3 worldPos, vec3 N, vec3 L, float viewDistance) {
    int cascadeIndex = 0;
    if (viewDistance > uShadow.uCascadeSplits.x) cascadeIndex = 1;
    if (viewDistance > uShadow.uCascadeSplits.y) cascadeIndex = 2;

    float shadowCurrent = SampleDirectionalShadowCascadeVSM(worldPos, N, L, cascadeIndex);
    if (cascadeIndex >= 2) {
        return shadowCurrent;
    }

    float prevSplit = (cascadeIndex == 0) ? 0.0 : uShadow.uCascadeSplits[cascadeIndex - 1];
    float nextSplit = uShadow.uCascadeSplits[cascadeIndex];
    float splitSpan = max(nextSplit - prevSplit, 0.001);
    float blendWidth = max(splitSpan * max(uShadow.uShadowMeta.w, 0.001), 0.1);
    float blendToNext = smoothstep(nextSplit - blendWidth, nextSplit + blendWidth, viewDistance);
    if (blendToNext <= 0.0) {
        return shadowCurrent;
    }

    float shadowNext = SampleDirectionalShadowCascadeVSM(worldPos, N, L, cascadeIndex + 1);
    return mix(shadowCurrent, shadowNext, blendToNext);
}

float SampleSpotShadowPCF(vec3 worldPos, vec3 N, vec3 L, int shadowIndex, mat4 lightViewProj) {
    if (shadowIndex < 0) return 1.0;
    
    // T1.1 Normal Offset Bias
    // Spot contact should stay grounded. Use a dedicated small normal bias budget.
    float normalBias = 0.0020 * max(1.0 - dot(N, L), 0.05);
    vec3 biasedPos = worldPos + N * normalBias;
    vec4 lightClip = lightViewProj * vec4(biasedPos, 1.0);
    if (lightClip.w <= 0.0) return 1.0;
    
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec2 shadowUv = lightNdc.xy * 0.5 + 0.5;
    float shadowDepth = ComputeStableReceiverDepth(lightNdc.z, N, L, 0.000035, 0.20, 0.35);
    
    if (shadowUv.x <= 0.0 || shadowUv.x >= 1.0 || shadowUv.y <= 0.0 || shadowUv.y >= 1.0 || shadowDepth <= 0.0 || shadowDepth >= 1.0) {
        return 1.0;
    }

    // Spot shadows use depth-only PCF (no VSM) for stable, solid contact and predictable behavior.
    return SampleDepthOnlyPcf2DArray(tSpotShadowMap, shadowUv, shadowIndex, shadowDepth, 1);
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
    
    vec3 V = normalize(uPerView.uViewPos.xyz - worldPos);
    float NdV = max(dot(N, V), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, clamp(metallic, 0.0, 1.0));
    vec3 Lo = vec3(0.0);

    // Directional Light
    {
        vec3 L = normalize(-uDirLight.uLightDir.xyz);
        float NdL = max(dot(N, L), 0.0);
        vec3 H = normalize(V + L);
        float VdH = max(dot(V, H), 0.0);
        vec3 radiance = uDirLight.uLightColorIntensity.rgb * uDirLight.uLightColorIntensity.a;
        float viewDistance = abs((uPerView.uView * vec4(worldPos, 1.0)).z);
        float shadow = SampleDirectionalShadowVSM(worldPos, N, L, viewDistance);

        vec3 F = FresnelSchlick(VdH, F0);
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 numerator = NDF * G * F;
        float denominator = max(4.0 * NdV * NdL, EPSILON);
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        Lo += (kD * albedo / PI + specular) * radiance * NdL * shadow;
    }

    // Spot Lights
    int spotCount = min(int(uSpotLights.countAndPadding.x), 16);
    for (int i = 0; i < spotCount; ++i) {
        SpotLight sl = uSpotLights.lights[i];
        vec3 lightPos = sl.positionAndRange.xyz;
        float range = sl.positionAndRange.w;
        vec3 lightDir = sl.directionAndInnerCone.xyz;
        float innerConeCos = sl.directionAndInnerCone.w;
        vec3 colorIntensity = sl.colorAndOuterCone.xyz;
        float outerConeCos = sl.colorAndOuterCone.w;

        vec3 L = lightPos - worldPos;
        float distance = length(L);
        if (distance > range) continue;

        L /= distance;
        float NdL = max(dot(N, L), 0.0);
        if (NdL <= 0.0) continue;

        float theta = dot(-L, lightDir);
        if (theta < outerConeCos) continue;

        float epsilon = max(innerConeCos - outerConeCos, EPSILON);
        float intensity = clamp((theta - outerConeCos) / epsilon, 0.0, 1.0);
        
        float attenuation = 1.0 - clamp(distance / range, 0.0, 1.0);
        attenuation *= attenuation;

        int shadowIndex = int(sl.shadowParams.x);
        float shadow = SampleSpotShadowPCF(worldPos, N, L, shadowIndex, sl.lightViewProj);

        vec3 radiance = colorIntensity * intensity * attenuation;

        vec3 H = normalize(V + L);
        float VdH = max(dot(V, H), 0.0);
        vec3 F = FresnelSchlick(VdH, F0);
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 numerator = NDF * G * F;
        float denominator = max(4.0 * NdV * NdL, EPSILON);
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        Lo += (kD * albedo / PI + specular) * radiance * NdL * shadow;
    }

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
    vec3 finalColor = Lo + ibl + emissive;
    outColor = vec4(finalColor, 1.0);
}
