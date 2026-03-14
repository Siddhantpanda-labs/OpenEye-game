#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_Texture;

layout(push_constant, std430) uniform FilmGrainPush {
    float u_Intensity;
    float u_Time;
} pc;

float Random(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233)) + pc.u_Time) * 43758.5453);
}

void main() {
    vec3 color = texture(u_Texture, v_TexCoord).rgb;
    float grain = Random(v_TexCoord * 1000.0) * 2.0 - 1.0;
    grain *= pc.u_Intensity;
    color += grain * 0.1;
    FragColor = vec4(color, 1.0);
}
