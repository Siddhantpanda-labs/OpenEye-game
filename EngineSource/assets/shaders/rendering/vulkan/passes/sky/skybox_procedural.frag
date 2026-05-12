// OpenEye next-gen rendering — sky pass (procedural variant) fragment shader.
// Analytic sky/ground gradient with a soft horizon band. No external textures.
//
// Logical asset path: passes/sky/skybox_procedural
// Layout: PipelineLayoutKind::PushOnly (no descriptor sets).

#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(push_constant, std430) uniform SkyboxProceduralPush {
    mat4  uInverseView;
    mat4  uInverseProjection;
    float uSkyboxBrightness;
} pc;

const vec3  kSkyColor         = vec3(0.75, 0.91, 0.99) * 1.5;
const vec3  kGroundColor      = vec3(0.40, 0.34, 0.20);
const vec3  kHorizonWhite     = vec3(1.0,  1.0,  1.0 );
const float kHorizonSharpness = 0.05;
const float kBaseBrightness   = 0.64;

void main() {
    vec2 ndc       = vUv * 2.0 - 1.0;
    vec4 clip      = vec4(ndc, 1.0, 1.0);
    vec4 viewDirH  = pc.uInverseProjection * clip;
    viewDirH      /= viewDirH.w;
    vec3 viewDir   = normalize(viewDirH.xyz);
    vec3 worldDir  = normalize((pc.uInverseView * vec4(viewDir, 0.0)).xyz);

    float t        = smoothstep(-kHorizonSharpness, kHorizonSharpness, worldDir.y);
    vec3  colour   = mix(kGroundColor, kSkyColor, t) * kBaseBrightness;

    float horizonBand = 1.0 - smoothstep(0.0, 0.1, abs(worldDir.y));
    colour = mix(colour, kHorizonWhite * kBaseBrightness, horizonBand * 0.1);

    colour *= max(pc.uSkyboxBrightness, 0.0);
    fragColor = vec4(colour, 1.0);
}
