/*
    OpenGL 4.1 Core - Gas Simulation: Buoyancy - Fragment Shader
*/

#version 410 core

out vec3 FragColor;

uniform sampler3D VelocityTexture;
uniform sampler3D TemperatureTexture;
uniform sampler3D DensityTexture;

uniform float ambientTemperature;
uniform float timeStep;
uniform float sigma;
uniform float kappa;
uniform vec3 InverseSize;

flat in float layer;

void main()
{
    vec3 fragCoord = vec3(gl_FragCoord.xy, layer);
    vec3 vel = texture(VelocityTexture, fragCoord * InverseSize).xyz;
    float T = texture(TemperatureTexture, fragCoord * InverseSize).x;
    float D = texture(DensityTexture, fragCoord * InverseSize).x;
    vec3 buoyancy = (sigma * (T - ambientTemperature) - kappa * D) * vec3(0.0, 1.0, 0.0);
    vel += timeStep * buoyancy;
    if (length(vel) < 0.0001) vel = vec3(0.0);
    FragColor = vel;
}


