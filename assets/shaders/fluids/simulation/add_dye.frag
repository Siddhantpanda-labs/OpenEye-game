/*
    OpenGL 4.1 Core - Fluid Simulation: Add Dye - Fragment Shader
*/

#version 410 core

out float FragColor;

uniform sampler3D DensityTexture;
uniform vec3 emitterPos;
uniform float emitterRadius;
uniform float densityValue;
uniform vec3 InverseSize;

in float layer;

void main()
{
    vec3 fragCoord = vec3(gl_FragCoord.xy, layer);
    vec3 pos = fragCoord * InverseSize;
    float d = distance(pos, emitterPos);
    float w = exp(- (d * d) / (2.0 * emitterRadius * emitterRadius));
    float dens = texture(DensityTexture, pos).x;
    FragColor = dens + w * densityValue;
}


