#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 0) uniform sampler2D u_HDRTexture;

layout(push_constant, std430) uniform BloomExtractPush {
    float u_Threshold;
} pc;

void main() {
    vec3 color = texture(u_HDRTexture, v_TexCoord).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 bloom = max(color - vec3(pc.u_Threshold), vec3(0.0));
    float smoothFactor = smoothstep(pc.u_Threshold, pc.u_Threshold * 1.5, luminance);
    bloom *= smoothFactor;
    FragColor = vec4(bloom, 1.0);
}
