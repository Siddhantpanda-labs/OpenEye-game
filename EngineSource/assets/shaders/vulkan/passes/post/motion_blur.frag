#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_Texture;
layout(binding = 1) uniform sampler2D g_Velocity;

layout(push_constant, std430) uniform MotionBlurPush {
    vec2 u_Resolution;
    float u_Intensity;
    int u_Samples;
} pc;

void main() {
    vec2 velocity = texture(g_Velocity, v_TexCoord).rg;
    if (length(velocity) < 0.001) {
        FragColor = texture(u_Texture, v_TexCoord);
        return;
    }
    vec3 color = texture(u_Texture, v_TexCoord).rgb;
    vec2 texelSize = 1.0 / pc.u_Resolution;
    vec2 offset = velocity * pc.u_Intensity * texelSize;
    float weight = 1.0 / float(pc.u_Samples + 1);
    color *= weight;
    for (int i = 1; i <= pc.u_Samples; ++i) {
        vec2 sampleCoord = v_TexCoord + offset * float(i);
        color += texture(u_Texture, sampleCoord).rgb * weight;
    }
    FragColor = vec4(color, 1.0);
}
