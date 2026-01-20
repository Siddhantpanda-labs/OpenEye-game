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
layout(location = 7) in int a_QuadIndex;         // Quad index (0=XY, 1=XZ, 2=YZ) for cross-billboard

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_CameraRight;   // Camera right vector (for billboard)
uniform vec3 u_CameraUp;       // Camera up vector (for billboard)
uniform vec3 u_CameraForward; // Camera forward vector (for cross-billboard)
uniform int u_RenderMode;     // 0=Billboard, 1=CrossBillboard, 2=Volumetric

out vec2 v_TexCoord;
out vec4 v_Color;
out float v_AgeNorm;

void main() {
    vec3 worldPos = a_ParticlePosition;
    vec2 finalPos = a_Position;
    
    if (u_RenderMode == 1) { // CrossBillboard
        int quadIndex = a_QuadIndex;
        
        // All quads face the camera - use cameraRight and cameraUp for all quads
        // Rotate the quad vertices around camera forward axis to create volumetric appearance
        vec3 cameraRight = normalize(u_CameraRight);
        vec3 cameraUp = normalize(u_CameraUp);
        vec3 cameraForward = normalize(u_CameraForward);
        
        // Rotate quad vertices around camera forward axis based on quadIndex
        // This creates 3 quads that all face the camera but are rotated around forward axis
        float angle = float(quadIndex) * 2.09439510239; // 120 degrees in radians (2*PI/3)
        float cosAngle = cos(angle);
        float sinAngle = sin(angle);
        
        // Rotate the quad position around camera forward
        vec2 rotatedQuadPos = vec2(
            a_Position.x * cosAngle - a_Position.y * sinAngle,
            a_Position.x * sinAngle + a_Position.y * cosAngle
        );
        
        // Apply particle rotation
        float cosRot = cos(a_ParticleRotation);
        float sinRot = sin(a_ParticleRotation);
        vec2 rotatedPos = vec2(
            rotatedQuadPos.x * cosRot - rotatedQuadPos.y * sinRot,
            rotatedQuadPos.x * sinRot + rotatedQuadPos.y * cosRot
        );
        
        // All quads use cameraRight and cameraUp (always face camera)
        worldPos += cameraRight * rotatedPos.x * a_ParticleSize + cameraUp * rotatedPos.y * a_ParticleSize;
        finalPos = rotatedPos;
    } else { // Billboard (default)
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
        
        worldPos += right * rotatedPos.x * a_ParticleSize + up * rotatedPos.y * a_ParticleSize;
        finalPos = rotatedPos;
    }
    
    // Transform to clip space
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
    
    // Pass through
    v_TexCoord = a_TexCoord;
    v_Color = a_ParticleColor;
    v_AgeNorm = a_ParticleAge;
}

