#version 450 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_Emission;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform vec3 u_ViewPos;
uniform float u_LightIntensity = 1.0;
uniform vec3 u_Ambient = vec3(0.1);
uniform float u_Exposure = 1.0;
uniform float u_Alpha = 1.0;

layout(binding = 5) uniform sampler2D u_ShadowMap;
uniform mat4 u_LightSpaceMatrix;
uniform float u_ShadowBias = 0.005;
uniform int u_UseShadows = 0;

out vec4 FragColor;

float shadowFactor(vec3 worldPos) {
    if (u_UseShadows <= 0) return 0.0;
    vec4 lightSpacePos = u_LightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
    ndc = ndc * 0.5 + 0.5;
    if (ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0)
        return 0.0;
    float depth = texture(u_ShadowMap, ndc.xy).r;
    return (ndc.z - u_ShadowBias > depth) ? 1.0 : 0.0;
}

void main() {
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(-u_LightDir);
    vec3 V = normalize(u_ViewPos - v_Position);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = shadowFactor(v_Position);
    float lightVis = 1.0 - shadow;
    vec3 diffuse = u_Albedo * u_LightColor * u_LightIntensity * NdotL * lightVis;
    vec3 ambient = u_Albedo * u_Ambient;
    vec3 color = (ambient + diffuse + u_Emission) * u_Exposure;
    FragColor = vec4(color, u_Alpha);
}


