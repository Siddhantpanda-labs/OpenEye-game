vec3 EvaluateBurleyBRDF(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L) {
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159 * denom * denom);
    float k = (a + 1.0); k = (k*k)/8.0;
    float Gl = NdotL / (NdotL * (1.0 - k) + k);
    float Gv = NdotV / (NdotV * (1.0 - k) + k);
    float G = Gl * Gv;
    vec3  F = F0 + (1.0 - F0) * pow(1.0 - LdotH, 5.0);
    vec3  specular = (D * G * F) / max(4.0 * NdotL * NdotV, 0.001);
    float Fd90 = 0.5 + 2.0 * LdotH * LdotH * roughness;
    float FdV = 1.0 + (Fd90 - 1.0) * pow(1.0 - NdotV, 5.0);
    float FdL = 1.0 + (Fd90 - 1.0) * pow(1.0 - NdotL, 5.0);
    vec3 kd = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kd * albedo * (FdV * FdL) / 3.14159;
    return diffuse + specular;
}

// Fresnel-Schlick with roughness for IBL
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}


