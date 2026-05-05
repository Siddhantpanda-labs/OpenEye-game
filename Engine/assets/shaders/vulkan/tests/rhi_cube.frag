#version 450

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 0) out vec4 outColor;

void main() {
    // Opaque white Blinn-Phong with reflective fresnel-like term.
    vec3 albedo = vec3(1.0);
    vec3 N = normalize(v_Normal);
    vec3 lightPos = vec3(2.0, 2.0, 2.0);
    vec3 viewPos = vec3(0.0, 0.0, 5.0);
    vec3 L = normalize(lightPos - v_WorldPos);
    vec3 V = normalize(viewPos - v_WorldPos);
    vec3 H = normalize(L + V);

    float dist = max(length(lightPos - v_WorldPos), 0.001);
    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
    float ndotl = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 128.0);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);

    vec3 ambient = 0.08 * albedo;
    vec3 diffuse = 1.6 * ndotl * albedo;
    vec3 specular = 2.8 * spec * vec3(1.0);
    vec3 reflective = 0.35 * fresnel * vec3(1.0);
    vec3 lit = ambient + attenuation * (diffuse + specular) + reflective;
    outColor = vec4(lit, 1.0);
}
