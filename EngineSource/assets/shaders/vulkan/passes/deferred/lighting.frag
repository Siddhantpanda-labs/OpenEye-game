#version 460
// Resource-space: set 0 = Frame (viewProj), set 1 = DeferredLightingPass (gbuffer 0-3, shadow 4, scene lighting 5, light space 6, IBL 7-9).
//
// Shadow debug (defaults off). Recompile SPIR-V after editing:
//   glslc EngineSource/assets/shaders/vulkan/passes/deferred/lighting.frag -o .../lighting.frag.spv
// Or: python EngineSource/tools/compile_vulkan_shaders.py
//
// OE_DEBUG_SHADOW_FORCE_CASCADE_LAYER: -1 = use normal farBounds cascade; 0..3 = always sample that layer.
// OE_DEBUG_SHADOW_VISUALIZE: 1 = bypass lighting, show raw shadow map depth (grayscale) for forced or cascade 0 UVs.
// OE_DEBUG_SHADOW_OOB_AS_SHADOW: 1 = UV/Z outside [0,1] counts as full shadow (was 0 = lit); use to see OOB rate.
// OE_DEBUG_SHADOW_INVERT_COMPARE: 1 = flip receiver-vs-depth test (Vulkan depth convention experiments).

#ifndef OE_DEBUG_SHADOW_FORCE_CASCADE_LAYER
#define OE_DEBUG_SHADOW_FORCE_CASCADE_LAYER -1
#endif
#ifndef OE_DEBUG_SHADOW_VISUALIZE
#define OE_DEBUG_SHADOW_VISUALIZE 0
#endif
#ifndef OE_DEBUG_SHADOW_OOB_AS_SHADOW
#define OE_DEBUG_SHADOW_OOB_AS_SHADOW 0
#endif
#ifndef OE_DEBUG_SHADOW_INVERT_COMPARE
#define OE_DEBUG_SHADOW_INVERT_COMPARE 0
#endif

layout(location = 0) in vec2 v_TexCoord;

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
#define MAX_CASCADES 4
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
    vec4 farBounds; // clip.z/clip.w thresholds (CPU: cascade far-plane center in ViewProj)
};
struct PointLightData {
    vec3 position; float pad0;
    vec3 color; float intensity;
    float radius; float pad1; float pad2; float pad3;
};
struct SpotLightData {
    vec3 position; float pad0;
    vec3 direction; float pad1;
    vec3 color; float intensity;
    float range; float innerConeCos; float outerConeCos;
    float pad2; float pad3; float pad4; float pad5; float pad6;
};

layout(set = 0, binding = 0) uniform FrameBlock {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProj;
    mat4 u_PrevView;
    mat4 u_PrevProjection;
};

layout(set = 1, binding = 0) uniform sampler2D u_Position;
layout(set = 1, binding = 1) uniform sampler2D u_Normal;
layout(set = 1, binding = 2) uniform sampler2D u_Albedo;
layout(set = 1, binding = 3) uniform sampler2D u_PBR;
layout(set = 1, binding = 4) uniform sampler2DArray u_ShadowMap;
layout(set = 1, binding = 5) buffer SceneLightingBlock {
    SceneLightHeader header;
    PointLightData pointLights[MAX_POINT_LIGHTS];
    SpotLightData spotLights[MAX_SPOT_LIGHTS];
} sceneLight;
layout(set = 1, binding = 6) uniform LightSpaceBlock {
    mat4 lightSpace[4];
};
layout(set = 1, binding = 7) uniform samplerCube u_IrradianceMap;
layout(set = 1, binding = 8) uniform samplerCube u_PrefilterMap;
layout(set = 1, binding = 9) uniform sampler2D u_BRDFLUT;

layout(push_constant) uniform LightingPush { int u_UseIBL; };

layout(location = 0) out vec4 o_Color;

// Fresnel-Schlick with roughness for IBL (from common/brdf.glsl)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float shadowFactor(vec3 worldPos) {
    int cascades = sceneLight.header.cascadeCount;
    if (cascades <= 0) return 0.0;
    vec4 clip = u_ViewProj * vec4(worldPos, 1.0);
    float clipZW = clip.z / clip.w; // same metric as CascadeShadowMap farBounds
    int c = 0;
    for (int i = 0; i < MAX_CASCADES - 1 && i < cascades; i++)
        if (clipZW > sceneLight.header.farBounds[i]) c = i + 1;
    c = min(c, cascades - 1);
#if OE_DEBUG_SHADOW_FORCE_CASCADE_LAYER >= 0
    c = min(OE_DEBUG_SHADOW_FORCE_CASCADE_LAYER, cascades - 1);
#endif
    vec4 lightSpacePos = lightSpace[c] * vec4(worldPos, 1.0);
    vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
    ndc = ndc * 0.5 + 0.5;
    if (ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0) {
#if OE_DEBUG_SHADOW_OOB_AS_SHADOW
        return 1.0;
#else
        return 0.0;
#endif
    }
    float depth = texture(u_ShadowMap, vec3(ndc.xy, float(c))).r;
    bool inShadow = (ndc.z - sceneLight.header.shadowBias > depth);
#if OE_DEBUG_SHADOW_INVERT_COMPARE
    inShadow = !inShadow;
#endif
    return inShadow ? 1.0 : 0.0;
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
    vec3 diff = albedo * (1.0 - metallic) * lightColor * intensity * NdotL * att;
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
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
    vec3 diff = albedo * (1.0 - metallic) * lightColor * intensity * NdotL * att;
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    diff += (0.04 + albedo * metallic) * lightColor * intensity * spec * att;
    return diff;
}

void main() {
    vec3 pos = texture(u_Position, v_TexCoord).rgb;
#if OE_DEBUG_SHADOW_VISUALIZE
    {
        int cascades = max(sceneLight.header.cascadeCount, 1);
        int layer = 0;
#if OE_DEBUG_SHADOW_FORCE_CASCADE_LAYER >= 0
        layer = min(OE_DEBUG_SHADOW_FORCE_CASCADE_LAYER, cascades - 1);
#endif
        vec4 lsp = lightSpace[layer] * vec4(pos, 1.0);
        vec3 ndc = lsp.xyz / max(abs(lsp.w), 1e-6);
        ndc = ndc * 0.5 + 0.5;
        vec2 uv = clamp(ndc.xy, vec2(0.001), vec2(0.999));
        float d = texture(u_ShadowMap, vec3(uv, float(layer))).r;
        o_Color = vec4(d, d, d, 1.0);
        return;
    }
#endif
    vec3 N = normalize(texture(u_Normal, v_TexCoord).rgb);
    vec3 albedo = texture(u_Albedo, v_TexCoord).rgb;
    vec2 mr = texture(u_PBR, v_TexCoord).rg;
    float metallic = mr.r;
    float roughness = max(mr.g, 0.04);
    roughness = max(roughness * 0.8, 0.04);

    if (metallic >= 2.0) {
        o_Color = vec4(albedo, 1.0);
        return;
    }

    vec3 V = normalize(sceneLight.header.viewPos - pos);
    vec3 L = normalize(-sceneLight.header.direction);
    float NdotL = max(dot(N, L), 0.0);

    float shadow = 0.0;
    if (sceneLight.header.useShadows > 0)
        shadow = shadowFactor(pos);
    float lightVis = 1.0 - shadow;

    vec3 diff = albedo * (1.0 - metallic) * sceneLight.header.ambient;
    diff += albedo * sceneLight.header.color * sceneLight.header.intensity * NdotL * lightVis;

    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, 64.0 * (1.0 - roughness));
    diff += (0.04 + albedo * metallic) * sceneLight.header.color * sceneLight.header.intensity * spec * lightVis;

    int numPoint = min(sceneLight.header.pointLightCount, MAX_POINT_LIGHTS);
    for (int i = 0; i < numPoint; i++) {
        diff += evalPointLight(pos, N, V, albedo, metallic, roughness,
            sceneLight.pointLights[i].position, sceneLight.pointLights[i].color,
            sceneLight.pointLights[i].intensity, sceneLight.pointLights[i].radius);
    }
    int numSpot = min(sceneLight.header.spotLightCount, MAX_SPOT_LIGHTS);
    for (int i = 0; i < numSpot; i++) {
        diff += evalSpotLight(pos, N, V, albedo, metallic, roughness,
            sceneLight.spotLights[i].position, normalize(sceneLight.spotLights[i].direction),
            sceneLight.spotLights[i].color, sceneLight.spotLights[i].intensity,
            sceneLight.spotLights[i].range,
            sceneLight.spotLights[i].innerConeCos, sceneLight.spotLights[i].outerConeCos);
    }

    if (u_UseIBL != 0) {
        float NdotV = max(dot(N, V), 0.0001);
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kD = (1.0 - F) * (1.0 - metallic);
        vec3 irradiance = texture(u_IrradianceMap, N).rgb;
        diff += kD * irradiance * albedo;
        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdfLUT = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
        diff += prefilteredColor * (F * brdfLUT.x + brdfLUT.y);
    }

    vec3 color = diff * sceneLight.header.exposure;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(lum), color, 1.15);
    color = clamp(color, vec3(0.0), vec3(10.0));

    o_Color = vec4(color, 1.0);
}
