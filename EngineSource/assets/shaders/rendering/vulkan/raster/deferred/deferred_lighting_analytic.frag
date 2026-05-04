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
layout(set = 1, binding = 1) uniform sampler2D tGBuffer1; // Normal(XYZ), Unused(A)
layout(set = 1, binding = 2) uniform sampler2D tGBuffer2; // Metallic(R), Roughness(G), AO(B), Mask(A)
// Depth from the G-buffer render target (same image written by gbuffer_static), not Main HDR depth.
layout(set = 1, binding = 3) uniform sampler2D tDepth;

layout(std140, set = 1, binding = 4) uniform DirLightUbo {
    vec4 uLightDir;
    vec4 uLightColorIntensity;
    vec4 uDlPad[14];
} uDirLight;

const float PI = 3.14159265358979323846;

// Temporary probe mode to narrow remaining deferred issues quickly:
// 0 = full lighting, 1 = depth grayscale, 2 = normal visualize, 3 = albedo visualize.
#ifndef OPENEYE_DEFERRED_LIGHTING_DEBUG_MODE
#define OPENEYE_DEFERRED_LIGHTING_DEBUG_MODE 1
#endif

// Function to reconstruct world position from depth
vec3 ReconstructWorldPos(vec2 uv, float depth, mat4 invViewProj) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = invViewProj * ndc;
    return world.xyz / world.w;
}

void main() {
    // Fullscreen-triangle VS emits vUv in [0,2]; texture() sampling expects normalized [0,1].
    const vec2 uv = vUv * 0.5;
    float depth = texture(tDepth, uv).r;

#if OPENEYE_DEFERRED_LIGHTING_DEBUG_MODE == 1
    outColor = vec4(vec3(depth), 1.0);
    return;
#endif

    vec4 gb0 = texture(tGBuffer0, uv);
    vec4 gb1 = texture(tGBuffer1, uv);

#if OPENEYE_DEFERRED_LIGHTING_DEBUG_MODE == 2
    outColor = vec4(normalize(gb1.xyz) * 0.5 + 0.5, 1.0);
    return;
#endif

#if OPENEYE_DEFERRED_LIGHTING_DEBUG_MODE == 3
    outColor = vec4(gb0.rgb, 1.0);
    return;
#endif

    if (depth >= 1.0) {
        // Skybox / background
        outColor = vec4(0.12, 0.14, 0.20, 1.0);
        return;
    }

    vec4 gb2 = texture(tGBuffer2, uv);

    vec3 albedo = gb0.rgb;
    vec3 N = normalize(gb1.xyz);
    float metallic = gb2.r;
    float roughness = gb2.g;
    float ao = gb2.b;

    mat4 invViewProj = inverse(uPerView.uViewProjection);
    vec3 worldPos = ReconstructWorldPos(uv, depth, invViewProj);
    
    vec3 L = normalize(-uDirLight.uLightDir.xyz);
    vec3 V = normalize(uPerView.uViewPos.xyz - worldPos);
    
    float NdL = max(dot(N, L), 0.0);
    vec3 radiance = uDirLight.uLightColorIntensity.rgb * uDirLight.uLightColorIntensity.a;

    vec3 diffuse = albedo * (1.0 / PI) * NdL * radiance;

    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), mix(4.0, 128.0, 1.0 - roughness)) * mix(0.04, 1.0, metallic);
    vec3 specCol = spec * radiance;

    float rim = pow(1.0 - max(dot(N, V), 0.0), 2.5) * 0.08;
    vec3 ambient = albedo * (0.04 + rim) * ao;

    vec3 Lo = ambient + diffuse + specCol;
    outColor = vec4(Lo, 1.0);
}
