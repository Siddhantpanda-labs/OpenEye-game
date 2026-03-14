#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_HDRTexture;

layout(push_constant, std430) uniform TonemapPush {
    float u_Exposure;
    int u_UseACES;
} pc;

vec3 ACESFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Reinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

void main() {
    vec3 hdrColor = texture(u_HDRTexture, v_TexCoord).rgb * pc.u_Exposure;
    vec3 ldrColor = (pc.u_UseACES != 0) ? ACESFilmic(hdrColor) : Reinhard(hdrColor);
    FragColor = vec4(ldrColor, 1.0);
}
