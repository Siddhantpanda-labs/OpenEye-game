#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform mat4 u_InverseViewMatrix;
uniform mat4 u_InverseProjectionMatrix;

// Default procedural sky:rgb(140, 178, 216) above, #967d4d below, soft horizon blend
const vec3 kSkyColor = vec3(0.75, 0.91, 0.99) * 2;        // #ccdee8
const vec3 kGroundColor = vec3(0.40, 0.34, 0.202);  //rgb(81, 71, 52)
const float kHorizonSharpness = 0.05;
const float kBrightness = 0.14;  // Scale down so tone-mapped result isn't too white

void main() {
    vec2 ndc = v_TexCoord * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 viewDirH = u_InverseProjectionMatrix * clip;
    viewDirH /= viewDirH.w;
    vec3 viewDir = normalize(viewDirH.xyz);
    vec3 worldDir = normalize((u_InverseViewMatrix * vec4(viewDir, 0.0)).xyz);

    // Blend sky (y > 0) and ground (y < 0) with smooth horizon
    float t = smoothstep(-kHorizonSharpness, kHorizonSharpness, worldDir.y);
    vec3 color = mix(kGroundColor, kSkyColor, t) * kBrightness;

    // Bright white strip at horizon, fast smooth fade up and down
    float distFromHorizon = abs(worldDir.y);
    float horizonBand = 1.0 - smoothstep(0.0, 0.1, distFromHorizon);
    vec3 horizonWhite = vec3(1.0, 1.0, 1.0);
    color = mix(color, horizonWhite * kBrightness, horizonBand * 0.1);

    FragColor = vec4(0.0,1.0,0.0, 1.0);
}
