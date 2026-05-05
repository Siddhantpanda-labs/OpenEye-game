#version 460

// Final LDR pass: linear display-referred RGB (0–1) → sRGB transfer for 8-bit UNORM output / swapchain blit.
layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 0) uniform sampler2D u_Texture;

vec3 LinearToSRGB(vec3 linear) {
    vec3 low = linear * 12.92;
    vec3 high = 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), linear));
}

void main() {
    vec3 linear = texture(u_Texture, v_TexCoord).rgb;
    FragColor = vec4(LinearToSRGB(clamp(linear, 0.0, 1.0)), 1.0);
}
