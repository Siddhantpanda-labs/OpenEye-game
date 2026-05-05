#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 0) uniform sampler2D u_Texture;

layout(push_constant, std430) uniform ChromaticPush {
    vec2 u_Resolution;
    float u_Intensity;
} pc;

void main() {
    vec2 center = vec2(0.5, 0.5);
    vec2 dist = v_TexCoord - center;
    float distFromCenter = length(dist);
    vec2 offsetDir = normalize(dist);
    float offset = pc.u_Intensity * distFromCenter;
    float r = texture(u_Texture, v_TexCoord + offsetDir * offset).r;
    float g = texture(u_Texture, v_TexCoord).g;
    float b = texture(u_Texture, v_TexCoord - offsetDir * offset * 0.5).b;
    FragColor = vec4(r, g, b, 1.0);
}
