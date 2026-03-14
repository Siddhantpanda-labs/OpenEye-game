#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(push_constant, std430) uniform SkyboxProceduralPush {
    mat4 u_InverseViewMatrix;
    mat4 u_InverseProjectionMatrix;
} pc;

const vec3 kSkyColor = vec3(0.75, 0.91, 0.99) * 2.0;
const vec3 kGroundColor = vec3(0.40, 0.34, 0.202);
const float kHorizonSharpness = 0.05;
const float kBrightness = 0.14;

void main() {
    vec2 ndc = v_TexCoord * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewDirH = pc.u_InverseProjectionMatrix * clip;
    viewDirH /= viewDirH.w;
    vec3 viewDir = normalize(viewDirH.xyz);
    vec3 worldDir = normalize((pc.u_InverseViewMatrix * vec4(viewDir, 0.0)).xyz);

    float t = smoothstep(-kHorizonSharpness, kHorizonSharpness, worldDir.y);
    vec3 color = mix(kGroundColor, kSkyColor, t) * kBrightness;

    float distFromHorizon = abs(worldDir.y);
    float horizonBand = 1.0 - smoothstep(0.0, 0.1, distFromHorizon);
    vec3 horizonWhite = vec3(1.0, 1.0, 1.0);
    color = mix(color, horizonWhite * kBrightness, horizonBand * 0.1);

    FragColor = vec4(color, 1.0);
}
