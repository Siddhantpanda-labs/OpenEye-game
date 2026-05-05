#version 460

// Scene color (HDR linear). Reinhard + sRGB encode for 8-bit UNORM swapchain.
layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 0) uniform sampler2D u_Texture;

vec3 LinearToSRGB(vec3 linear) {
    vec3 low = linear * 12.92;
    vec3 high = 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), linear));
}

void main() {
    vec3 hdr = texture(u_Texture, v_TexCoord).rgb;
    vec3 linear = hdr / (hdr + vec3(1.0));
    linear = clamp(linear, 0.0, 1.0);
    FragColor = vec4(LinearToSRGB(linear), 1.0);
}
