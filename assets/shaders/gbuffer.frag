#version 450 core

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoord;

// G-Buffer outputs (Multiple Render Targets)
layout(location = 0) out vec3 g_Position;      // World space position
layout(location = 1) out vec3 g_Normal;        // World space normal
layout(location = 2) out vec3 g_Albedo;        // Base color
layout(location = 3) out vec2 g_PBRParams;     // Metallic (R), Roughness (G)

uniform vec3 u_Albedo = vec3(0.8);
uniform float u_Metallic = 0.0;
uniform float u_Roughness = 0.5;

void main() {
    // Discard fragments with invalid positions (safety check)
    // This shouldn't happen but prevents writing invalid data
    
    // Store fragment's world-space position
    g_Position = v_Position;
    
    // Store fragment's normal (normalized) - ensure it's valid
    vec3 normal = normalize(v_Normal);
    if (isnan(normal.x) || isnan(normal.y) || isnan(normal.z)) {
        normal = vec3(0.0, 1.0, 0.0); // Fallback to up
    }
    g_Normal = normal;
    
    // Store albedo (base color) - ensure valid range
    g_Albedo = clamp(u_Albedo, vec3(0.0), vec3(1.0));
    
    // Store PBR parameters
    g_PBRParams = vec2(clamp(u_Metallic, 0.0, 1.0), clamp(u_Roughness, 0.04, 1.0));
}

