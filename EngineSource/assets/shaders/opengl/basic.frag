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

out vec4 FragColor;

void main() {
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(-u_LightDir);
    vec3 V = normalize(u_ViewPos - v_Position);
    
    // Simple Lambertian diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = u_Albedo * u_LightColor * NdotL;
    
    // Simple ambient
    vec3 ambient = u_Albedo * 0.1;
    
    // Combine
    vec3 color = ambient + diffuse + u_Emission;
    
    FragColor = vec4(color, 1.0);
}

