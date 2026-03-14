#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_Texture;

layout(push_constant, std430) uniform BloomBlurPush {
    vec2 u_Resolution;
    int u_Horizontal;
    float u_Intensity;
} pc;

void main() {
    vec2 texelSize = 1.0 / pc.u_Resolution;
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(u_Texture, v_TexCoord).rgb * weights[0];
    float blurRadius = pc.u_Intensity;
    if (pc.u_Horizontal != 0) {
        for (int i = 1; i < 5; ++i) {
            float offset = float(i) * blurRadius;
            result += texture(u_Texture, v_TexCoord + vec2(texelSize.x * offset, 0.0)).rgb * weights[i];
            result += texture(u_Texture, v_TexCoord - vec2(texelSize.x * offset, 0.0)).rgb * weights[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            float offset = float(i) * blurRadius;
            result += texture(u_Texture, v_TexCoord + vec2(0.0, texelSize.y * offset)).rgb * weights[i];
            result += texture(u_Texture, v_TexCoord - vec2(0.0, texelSize.y * offset)).rgb * weights[i];
        }
    }
    FragColor = vec4(result, 1.0);
}
