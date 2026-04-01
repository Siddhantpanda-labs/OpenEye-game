#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 1, binding = 0) uniform samplerCube u_Skybox;

layout(push_constant, std430) uniform SkyboxPush {
    mat4 u_InverseViewMatrix;
    mat4 u_InverseProjectionMatrix;
    float u_SkyboxBrightness;
} pc;

void main() {
    vec2 ndc = v_TexCoord * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewDirH = pc.u_InverseProjectionMatrix * clip;
    viewDirH /= viewDirH.w;
    vec3 viewDir = normalize(viewDirH.xyz);
    vec3 worldDir = normalize((pc.u_InverseViewMatrix * vec4(viewDir, 0.0)).xyz);
    vec3 color = texture(u_Skybox, worldDir).rgb;
    color *= max(pc.u_SkyboxBrightness, 0.0);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
