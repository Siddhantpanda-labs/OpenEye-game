#version 430 core

in vec2 v_TexCoord;
layout(location=0) out vec4 o_Color;

uniform sampler2D g_Position;
uniform sampler2D g_Normal;
uniform sampler2D g_Albedo;
uniform sampler2D u_SceneTexture; // Current scene render
uniform samplerCube u_Skybox; // Fallback for environment
uniform vec2 u_Resolution;
uniform float u_StepSize = 0.1;
uniform int u_MaxSteps = 32;
uniform float u_Thickness = 0.01;
uniform mat4 u_ViewMatrix;
uniform mat4 u_ProjectionMatrix;
uniform mat4 u_InverseViewMatrix;
uniform mat4 u_InverseProjectionMatrix;
uniform vec3 u_ViewPos;

vec3 RayMarch(vec3 rayOrigin, vec3 rayDir, float rayLength) {
    float stepSize = u_StepSize;
    int maxSteps = u_MaxSteps;
    
    vec3 currentPos = rayOrigin;
    
    for (int i = 0; i < maxSteps; ++i) {
        currentPos += rayDir * stepSize;
        
        // Project to screen space
        vec4 projected = u_ProjectionMatrix * vec4(currentPos, 1.0);
        projected.xy /= projected.w;
        vec2 screenCoord = projected.xy * 0.5 + 0.5;
        
        // Check bounds
        if (screenCoord.x < 0.0 || screenCoord.x > 1.0 || screenCoord.y < 0.0 || screenCoord.y > 1.0) {
            break;
        }
        
        // Sample position at this screen coordinate
        vec3 samplePos = texture(g_Position, screenCoord).rgb;
        
        if (length(samplePos) < 0.001) {
            continue; // Skip sky
        }
        
        // Check if we hit something (compare view-space Z)
        float depthDiff = abs(currentPos.z - samplePos.z);
        if (depthDiff > 0.0 && depthDiff < u_Thickness) {
            // Hit! Return the albedo at this position
            return texture(g_Albedo, screenCoord).rgb;
        }
    }
    
    // No hit - will sample skybox in main()
    return vec3(-1.0); // Signal for skybox fallback
}

void main() {
    vec3 position = texture(g_Position, v_TexCoord).rgb;
    vec3 normal = normalize(texture(g_Normal, v_TexCoord).rgb);
    vec3 sceneColor = texture(u_SceneTexture, v_TexCoord).rgb;
    
    // Skip SSR for sky/background
    if (length(position) < 0.001) {
        o_Color = vec4(sceneColor, 1.0);
        return;
    }
    
    // Calculate reflection direction in view space
    vec3 viewDir = normalize(u_ViewPos - position);
    vec3 reflectDir = reflect(-viewDir, normal);
    
    // Ray march in view space
    vec3 reflectionColor = RayMarch(position, reflectDir, 10.0);
    
    // If ray march didn't hit anything, sample skybox
    if (reflectionColor.r < -0.5) {
        // Transform reflection direction to world space for skybox sampling
        vec3 worldReflectDir = mat3(u_InverseViewMatrix) * reflectDir;
        reflectionColor = texture(u_Skybox, worldReflectDir).rgb;
    }
    
    // If still no valid color, use scene color
    if (reflectionColor.r < -0.5) {
        reflectionColor = sceneColor;
    }
    
    // Calculate fresnel for blending (more reflection at glancing angles)
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.0);
    
    // Blend reflection with scene (fresnel controls blend amount)
    vec3 finalColor = mix(sceneColor, reflectionColor, fresnel * 0.5);
    
    o_Color = vec4(finalColor, 1.0);
}
