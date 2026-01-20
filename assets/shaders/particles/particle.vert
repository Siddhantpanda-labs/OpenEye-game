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
        
        // All quads MUST face the camera - use cameraRight and cameraUp directly
        vec3 cameraRight = normalize(u_CameraRight);
        vec3 cameraUp = normalize(u_CameraUp);
        
        // Apply particle rotation to quad vertices
        float cosRot = cos(a_ParticleRotation);
        float sinRot = sin(a_ParticleRotation);
        vec2 rotatedPos = vec2(
            a_Position.x * cosRot - a_Position.y * sinRot,
            a_Position.x * sinRot + a_Position.y * cosRot
        );
        
        // Rotate the quad vertices in 2D space around the center based on quadIndex
        // This creates 3 quads that all face the camera but are rotated differently
        float quadRotation = float(quadIndex) * 2.09439510239; // 120 degrees in radians
        float cosQuadRot = cos(quadRotation);
        float sinQuadRot = sin(quadRotation);
        vec2 finalRotatedPos = vec2(
            rotatedPos.x * cosQuadRot - rotatedPos.y * sinQuadRot,
            rotatedPos.x * sinQuadRot + rotatedPos.y * cosQuadRot
        );
        
        // All quads use cameraRight and cameraUp (always face camera)
        worldPos += cameraRight * finalRotatedPos.x * a_ParticleSize + cameraUp * finalRotatedPos.y * a_ParticleSize;
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

