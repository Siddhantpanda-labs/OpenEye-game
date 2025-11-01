#version 450 core

out vec4 FragColor;

in vec2 v_TexCoord;

// G-Buffer textures
uniform sampler2D g_Position;
uniform sampler2D g_Normal;
uniform sampler2D g_Albedo;
uniform sampler2D g_PBRParams;

// Shadow maps (cascaded)
#define MAX_CASCADES 4
uniform int u_ShadowCascadeCount = 4;
uniform sampler2D u_ShadowMaps[MAX_CASCADES];
uniform mat4 u_ShadowMatrices[MAX_CASCADES];
uniform float u_CascadeSplits[MAX_CASCADES + 1];

// Camera
uniform vec3 u_ViewPos;
uniform mat4 u_ViewMatrix;
uniform mat4 u_ProjectionMatrix;

// Directional light
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_LightIntensity = 1.0;

// Point lights (max 32 for now)
#define MAX_POINT_LIGHTS 32
uniform int u_PointLightCount = 0;
uniform vec3 u_PointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 u_PointLightColors[MAX_POINT_LIGHTS];
uniform float u_PointLightIntensities[MAX_POINT_LIGHTS];
uniform float u_PointLightRanges[MAX_POINT_LIGHTS];

// Simple PBR calculation
vec3 CalculatePBR(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 lightColor) {
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 H = normalize(V + L);
    
    // Cook-Torrance BRDF approximation
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    
    // Roughness-based specular
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    
    // Fresnel-Schlick approximation
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
    
    // Simple diffuse + specular
    vec3 diffuse = albedo / 3.14159; // Lambertian diffuse
    vec3 specular = F * (alpha + 2.0) / 8.0 * pow(NdotH, alpha * 4.0 + 2.0);
    
    return (diffuse * (1.0 - metallic) + specular * metallic) * lightColor * NdotL;
}

// PCF shadow calculation
float CalculateShadow(vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir) {
    if (u_ShadowCascadeCount <= 0) {
        return 1.0;
    }
    
    // Determine which cascade to use
    vec4 fragPosViewSpace = u_ViewMatrix * vec4(fragPosWorldSpace, 1.0);
    float viewDepth = -fragPosViewSpace.z;
    
    int cascadeIndex = 0;
    for (int i = 0; i < u_ShadowCascadeCount - 1; i++) {
        if (viewDepth > u_CascadeSplits[i + 1]) {
            cascadeIndex = i + 1;
        }
    }
    
    if (cascadeIndex >= u_ShadowCascadeCount) {
        cascadeIndex = u_ShadowCascadeCount - 1;
    }
    
    // Transform to shadow space
    vec4 fragPosLightSpace = u_ShadowMatrices[cascadeIndex] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if in shadow map bounds
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0; // Outside shadow map, consider lit
    }
    
    // PCF filtering (3x3 kernel)
    float shadow = 0.0;
    float currentDepth = projCoords.z;
    float bias = 0.005;
    
    vec2 texelSize = 1.0 / textureSize(u_ShadowMaps[cascadeIndex], 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(u_ShadowMaps[cascadeIndex], 
                                    projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    return 1.0 - shadow;
}

void main() {
    // Retrieve G-Buffer data
    vec3 position = texture(g_Position, v_TexCoord).rgb;
    vec3 normal = normalize(texture(g_Normal, v_TexCoord).rgb);
    vec3 albedo = texture(g_Albedo, v_TexCoord).rgb;
    vec2 pbrParams = texture(g_PBRParams, v_TexCoord).rg;
    
    float metallic = pbrParams.r;
    float roughness = max(pbrParams.g, 0.04); // Clamp minimum roughness
    
    // TEMPORARY DEBUG: Visualize G-Buffer position to see if data is being written
    // If you see colored areas, G-Buffer is working. If all dark blue, G-Buffer is empty.
    float posLen = length(position);
    if (posLen < 0.001) {
        FragColor = vec4(0.05, 0.05, 0.1, 1.0); // Dark blue background (no geometry)
        return;
    }
    
    // DEBUG: Uncomment these lines to visualize G-Buffer data instead of lighting
    // FragColor = vec4(abs(position) * 0.1, 1.0); // Visualize position as color
    // FragColor = vec4(normal * 0.5 + 0.5, 1.0); // Visualize normal as color
    // FragColor = vec4(albedo, 1.0); // Visualize albedo directly
    // return; // Return early to skip lighting calculation
    
    // View direction
    vec3 viewDir = normalize(u_ViewPos - position);
    
    // Accumulate lighting
    vec3 lighting = vec3(0.0);
    
    // Directional light with shadows
    if (u_LightIntensity > 0.0) {
        vec3 lightDir = normalize(-u_LightDir);
        float shadow = CalculateShadow(position, normal, lightDir);
        vec3 lightContribution = CalculatePBR(albedo, metallic, roughness, normal, viewDir, lightDir, u_LightColor) * u_LightIntensity;
        lighting += lightContribution * shadow;
    }
    
    // Point lights
    for (int i = 0; i < u_PointLightCount && i < MAX_POINT_LIGHTS; i++) {
        vec3 lightPos = u_PointLightPositions[i];
        vec3 lightDir = normalize(lightPos - position);
        float distance = length(lightPos - position);
        
        // Attenuation
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
        float rangeFactor = 1.0 - smoothstep(0.0, u_PointLightRanges[i], distance);
        
        lighting += CalculatePBR(albedo, metallic, roughness, normal, viewDir, lightDir, u_PointLightColors[i]) 
                   * u_PointLightIntensities[i] * attenuation * rangeFactor;
    }
    
    // Ambient (simple) - increase for visibility
    vec3 ambient = albedo * 0.15;
    
    FragColor = vec4(lighting + ambient, 1.0); 

}

