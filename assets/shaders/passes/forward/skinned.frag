#version 450 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform sampler2D u_AlbedoTexture;
uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_Emission;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform vec3 u_ViewPos;

out vec4 FragColor;

void main() {
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(-u_LightDir);
    vec3 texColor = texture(u_AlbedoTexture, v_TexCoord).rgb;
    vec3 albedo = texColor * u_Albedo;
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = albedo * u_LightColor * NdotL;
    vec3 ambient = albedo * 0.1;
    vec3 color = ambient + diffuse + u_Emission;
    FragColor = vec4(color, 1.0);
}
