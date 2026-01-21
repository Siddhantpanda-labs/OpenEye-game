/*
    OpenGL 4.1 Core - Fluid Simulation: Semi-Lagrangian Advection - Fragment Shader
*/

#version 410 core

out vec4 FragColor; // Output data

// Input texture samplers
uniform sampler3D VelocityTexture; 
uniform sampler3D SourceTexture;
uniform sampler3D ObstacleTexture;

uniform float timeStep; // Time step
uniform vec3 InverseSize; // Inverse size of the simulation grid
uniform float dissipation; // Dissipation factor

flat in float layer; // Layer of the 3D texture

void main()
{
    vec3 fragCoord = vec3(gl_FragCoord.xy, layer);
    float obstacle = texture(ObstacleTexture, InverseSize * fragCoord).r;
    vec4 finalColor = vec4(0.0);
    if (obstacle < 1.0)
    {
        vec3 u = texture(VelocityTexture, InverseSize * fragCoord).xyz;
        vec3 coord = InverseSize * (fragCoord - timeStep * u);
        finalColor = dissipation * texture(SourceTexture, coord);
        if (length(finalColor) < 0.0001)
            finalColor = vec4(0.0);
    }
    FragColor = finalColor;
}


