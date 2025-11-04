#version 450 core

// Quad vertices
layout(location = 0) in vec2 a_Position;      // Quad vertex position
layout(location = 1) in vec2 a_TexCoord;      // Quad texture coordinates

// Instance data (per-particle)
layout(location = 2) in vec3 a_ParticlePosition;  // Particle world position
layout(location = 3) in float a_ParticleSize;     // Particle size
layout(location = 4) in vec4 a_ParticleColor;     // Particle color (RGBA)
layout(location = 5) in float a_ParticleRotation; // Particle rotation

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_CameraRight;   // Camera right vector (for billboard)
uniform vec3 u_CameraUp;       // Camera up vector (for billboard)

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
    // Billboard calculation: align quad to face camera
    vec3 right = normalize(u_CameraRight);
    vec3 up = normalize(u_CameraUp);
    
    // Rotate quad around its center
    float cosRot = cos(a_ParticleRotation);
    float sinRot = sin(a_ParticleRotation);
    vec2 rotatedPos = vec2(
        a_Position.x * cosRot - a_Position.y * sinRot,
        a_Position.x * sinRot + a_Position.y * cosRot
    );
    
    // Calculate billboard position
    vec3 worldPos = a_ParticlePosition + 
                    right * rotatedPos.x * a_ParticleSize +
                    up * rotatedPos.y * a_ParticleSize;
    
    // Transform to clip space
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
    
    // Pass through
    v_TexCoord = a_TexCoord;
    v_Color = a_ParticleColor;
}

