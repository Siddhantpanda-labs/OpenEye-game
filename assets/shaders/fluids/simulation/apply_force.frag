/*
    OpenGL 4.1 Core - Fluid Simulation: Application of External Forces - Fragment Shader
*/

#version 410 core

out vec4 FragColor;

uniform sampler3D VelocityTexture;
uniform vec3 InverseSize;

uniform vec3 force; // Wind force direction and strength
uniform vec3 center; // Center of the force application (use emitter pos for wind)
uniform float radius; // Radius of the force application

in float layer;

void main()
{
    vec3 fragCoord = vec3(gl_FragCoord.xy, layer);
    vec3 pos = fragCoord * InverseSize;
    
    // Calculate distance from center
    vec3 d = pos - center;
    float distSq = dot(d, d);
    
    // Gaussian splat for smooth force application
    float gaussianSplat = exp(-distSq / (radius * radius * 2.0));
    
    // Apply wind force scaled by distance
    vec3 vForce = force * gaussianSplat;
    
    // Add to current velocity
    vec3 currentVel = texture(VelocityTexture, pos).xyz;
    vec3 finalVel = currentVel + vForce;
    
    // Clamp very small velocities to zero
    if (length(finalVel) < 0.0001)
        finalVel = vec3(0.0);
    
    FragColor = vec4(finalVel, 1.0);
}

