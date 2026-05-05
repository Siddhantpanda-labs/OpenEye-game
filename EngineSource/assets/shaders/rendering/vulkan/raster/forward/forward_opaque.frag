// OpenEye next-gen rendering — M5 forward opaque fragment (minimal analytic direct + specular + ambient).
// Matches DirectionalLightStd140 / PerViewUniformStd140 on set0 binding 1; directional light set1 binding 1.
#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldN;
layout(location = 2) in vec2 vUv;
layout(location = 3) flat in uint vMaterialId;
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

layout(std140, set = 1, binding = 1) uniform DirLightUbo {
    vec4 uLightDir;
    vec4 uLightColorIntensity;
    vec4 uDlPad[14];
} uDirLight;

const float PI = 3.14159265358979323846;

void main() {
    vec3 N = normalize(vWorldN);
    vec3 L = normalize(-uDirLight.uLightDir.xyz);
    float NdL = max(dot(N, L), 0.0);
    vec3 radiance = uDirLight.uLightColorIntensity.rgb * uDirLight.uLightColorIntensity.a;

    vec3 albedo = vec3(0.72, 0.45, 0.32);
    if (vMaterialId == 3u)
        albedo = vec3(1.0, 0.92, 0.18); // bright yellow debug quad
    else if ((vMaterialId & 1u) != 0u)
        albedo = vec3(0.32, 0.62, 0.88);

    vec3 diffuse = albedo * (1.0 / PI) * NdL * radiance;

    vec3 V = normalize(uPerView.uViewPos.xyz - vWorldPos);
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), 48.0) * 0.2;
    vec3 specCol = spec * radiance;

    float rim = pow(1.0 - max(dot(N, V), 0.0), 2.5) * 0.08;
    vec3 ambient = albedo * (0.04 + rim);

    vec3 Lo = ambient + diffuse + specCol;
    outColor = vec4(Lo, 1.0);
    // outColor = vec4(20.0, 20.0, 0.0, 1.0);
}
