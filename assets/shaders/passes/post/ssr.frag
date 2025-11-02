#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D g_Position;
uniform sampler2D g_Normal;
uniform sampler2D g_Albedo;
uniform sampler2D u_SceneTexture; // Current scene render
uniform samplerCube u_Skybox; // Fallback for environment
uniform vec2 u_Resolution;
uniform float u_StepSize = 0.1;
uniform int u_MaxSteps = 32;
uniform float u_Thickness = 0.01;

#include "../../common/camera.glsl"

vec3 RayMarch(vec3 rayOrigin, vec3 rayDir, float rayLength) {
    float stepSize = u_StepSize;
    int maxSteps = u_MaxSteps;
    
    vec3 currentPos = rayOrigin;
    float currentDepth = rayOrigin.z;
    
    for (int i = 0; i < maxSteps; ++i) {
        currentPos += rayDir * stepSize;
        
        // Project to screen space
        vec4 projected = u_Projection * vec4(currentPos, 1.0);
        projected.xy /= projected.w;
        vec2 screenCoord = projected.xy * 0.5 + 0.5;
        
        // Check bounds
        if (screenCoord.x < 0.0 || screenCoord.x > 1.0 || screenCoord.y < 0.0 || screenCoord.y > 1.0) {
            break;
        }
        
        // Sample depth at this position
        vec3 samplePos = texture(g_Position, screenCoord).rgb;
        
        if (length(samplePos) < 0.001) {
            continue; // Skip sky
        }
        
        // Check if we hit something
        float depthDiff = currentPos.z - samplePos.z;
        if (depthDiff > 0.0 && depthDiff < u_Thickness) {
            // Hit! Return the albedo at this position
            return texture(g_Albedo, screenCoord).rgb;
        }
    }
    
    // No hit - return skybox
    return vec3(0.0); // Will be blended with scene
}

void main() {
    vec3 position = texture(g_Position, v_TexCoord).rgb;
    vec3 normal = normalize(texture(g_Normal, v_TexCoord).rgb);
    vec3 sceneColor = texture(u_SceneTexture, v_TexCoord).rgb;
    
    // Skip SSR for sky/background
    if (length(position) < 0.001) {
        FragColor = vec4(sceneColor, 1.0);
        return;
    }
    
    // Calculate reflection
    vec3 viewDir = normalize(u_ViewPos - position);
    vec3 reflectDir = reflect(-viewDir, normal);
    
    // Ray march
    vec3 reflectionColor = RayMarch(position, reflectDir, 10.0);
    
    // Calculate fresnel for blending
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.0);
    
    // Blend reflection with scene
    vec3 finalColor = mix(sceneColor, reflectionColor, fresnel * 0.5);
    
    FragColor = vec4(finalColor, 1.0);
}

