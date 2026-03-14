#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_Texture;

layout(push_constant, std430) uniform VignettePush {
    float u_Intensity;
    float u_Radius;
    float u_Softness;
} pc;

void main() {
    vec3 color = texture(u_Texture, v_TexCoord).rgb;
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(v_TexCoord, center);
    float vignette = smoothstep(pc.u_Radius, pc.u_Radius + pc.u_Softness, dist);
    vignette = 1.0 - (vignette * pc.u_Intensity);
    color *= vignette;
    FragColor = vec4(color, 1.0);
}
