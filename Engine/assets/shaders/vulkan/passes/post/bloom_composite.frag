#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_SceneTexture;
layout(binding = 1) uniform sampler2D u_BloomTexture;

layout(push_constant, std430) uniform BloomCompositePush {
    float u_BloomIntensity;
} pc;

void main() {
    vec3 scene = texture(u_SceneTexture, v_TexCoord).rgb;
    vec3 bloom = texture(u_BloomTexture, v_TexCoord).rgb;
    FragColor = vec4(scene + bloom * pc.u_BloomIntensity, 1.0);
}
