// OpenEye next-gen rendering — sky pass (cubemap variant) fragment shader.
// Samples a per-scene environment cubemap at the world-space ray direction reconstructed
// from inverse view/projection push constants.
//
// Parity with legacy `assets/shaders/vulkan/passes/sky/skybox.frag`: after linear
// `samplerCube` decode (sRGB cubemap → linear) and brightness, apply gamma encode
// `pow(rgb, 1/2.2)` before writing. That matches the old Vulkan sky look (mid-tones read
// brighter through the same Reinhard + sRGB `present_resolve` path as the rest of the
// frame). See also `passes/post/present_resolve.frag`.
//
// Logical asset path: passes/sky/skybox_cubemap
// Layout: PipelineLayoutKind::IBL (set 0 binding 0 = samplerCube).

#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform samplerCube uSkybox;

layout(push_constant, std430) uniform SkyboxPush {
    mat4  uInverseView;
    mat4  uInverseProjection;
    float uSkyboxBrightness;
} pc;

void main() {
    vec2 ndc       = vUv * 2.0 - 1.0;
    vec4 clip      = vec4(ndc, 1.0, 1.0);
    vec4 viewDirH  = pc.uInverseProjection * clip;
    viewDirH      /= viewDirH.w;
    vec3 viewDir   = normalize(viewDirH.xyz);
    vec3 worldDir  = normalize((pc.uInverseView * vec4(viewDir, 0.0)).xyz);

    vec3 colour = texture(uSkybox, worldDir).rgb;
    colour *= max(pc.uSkyboxBrightness, 0.0);
    colour = pow(colour, vec3(1.0 / 2.2));
    fragColor = vec4(colour, 1.0);
}
