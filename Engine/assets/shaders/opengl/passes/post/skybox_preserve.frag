#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_SceneLDR;
uniform sampler2D u_Depth;
uniform samplerCube u_Skybox;
uniform mat4 u_InverseViewMatrix;
uniform mat4 u_InverseProjectionMatrix;

void main() {
    vec3 sceneColor = texture(u_SceneLDR, v_TexCoord).rgb;
    float depth = texture(u_Depth, v_TexCoord).r;
    if (depth < 0.999999) {
        FragColor = vec4(sceneColor, 1.0);
        return;
    }

    vec2 ndc = v_TexCoord * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewDirH = u_InverseProjectionMatrix * clip;
    viewDirH /= max(viewDirH.w, 1e-6);
    vec3 viewDir = normalize(viewDirH.xyz);
    vec3 worldDir = normalize((u_InverseViewMatrix * vec4(viewDir, 0.0)).xyz);
    vec3 skyColor = texture(u_Skybox, worldDir).rgb;
    FragColor = vec4(skyColor, 1.0);
}
