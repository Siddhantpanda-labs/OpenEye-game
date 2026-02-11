#version 450 core

// Quad vertices
layout(location = 0) in vec2 a_Position;      // Quad vertex position
layout(location = 1) in vec2 a_TexCoord;      // Quad texture coordinates

// Instance data (per-particle)
layout(location = 2) in vec3 a_ParticlePosition;  // Particle world position
layout(location = 3) in float a_ParticleSize;     // Particle size
layout(location = 4) in vec4 a_ParticleColor;     // Particle color (RGBA)
layout(location = 5) in float a_ParticleRotation; // Particle rotation
layout(location = 6) in float a_ParticleAge;     // Normalized age 0..1

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_CameraRight;   // Camera right vector
uniform vec3 u_CameraUp;       // Camera up vector
uniform vec3 u_CameraForward; // Camera forward vector
uniform vec3 u_CameraPos;      // Camera position in world space

out vec2 v_TexCoord;
out vec4 v_Color;
out float v_AgeNorm;
out vec3 v_WorldPos;      // World position of quad center
out vec3 v_ViewDir;        // View direction from camera to particle
out float v_ParticleSize;  // Particle size for raymarching

void main() {
    // Standard billboard calculation: align quad to face camera
    vec3 right = normalize(u_CameraRight);
    vec3 up = normalize(u_CameraUp);
    
    // Rotate quad around its center
    float cosRot = cos(a_ParticleRotation);
    float sinRot = sin(a_ParticleRotation);
    vec2 rotatedPos = vec2(
        a_Position.x * cosRot - a_Position.y * sinRot,
        a_Position.x * sinRot + a_Position.y * cosRot
    );
    
    // Calculate billboard position (expanded quad for raymarching)
    vec3 worldPos = a_ParticlePosition + 
                    right * rotatedPos.x * a_ParticleSize * 2.0 + // Expand quad for raymarching
                    up * rotatedPos.y * a_ParticleSize * 2.0;
    
    // Transform to clip space
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
    
    // Pass through
    v_TexCoord = a_TexCoord;
    v_Color = a_ParticleColor;
    v_AgeNorm = a_ParticleAge;
    v_WorldPos = a_ParticlePosition;
    v_ViewDir = normalize(a_ParticlePosition - u_CameraPos);
    v_ParticleSize = a_ParticleSize;
}
