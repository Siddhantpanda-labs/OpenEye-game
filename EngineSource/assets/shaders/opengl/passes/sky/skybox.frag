#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform samplerCube u_Skybox;
uniform mat4 u_InverseViewMatrix;
uniform mat4 u_InverseProjectionMatrix;

// Preserve actual color: full saturation, neutral contrast (no wash-out)
const float kSaturation = 1.0;
const float kContrast = 1.0;

void main() {
    vec2 ndc = v_TexCoord * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewDirH = u_InverseProjectionMatrix * clip;
    viewDirH /= viewDirH.w;
    vec3 viewDir = normalize(viewDirH.xyz);
    vec3 worldDir = normalize((u_InverseViewMatrix * vec4(viewDir, 0.0)).xyz);
    vec3 color = texture(u_Skybox, worldDir).rgb;

    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, kSaturation);
    color = (color - 0.5) * kContrast + 0.5;

    FragColor = vec4(color, 1.0);
}


