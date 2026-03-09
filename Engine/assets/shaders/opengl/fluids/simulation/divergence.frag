/*
    OpenGL 4.1 Core - Fluid Simulation: Divergence - Fragment Shader
*/

#version 410 core

out float FragColor; // Output data

uniform sampler3D VelocityTexture;
uniform sampler3D ObstacleTexture;
uniform sampler3D ObstacleVelocityTexture;
uniform vec3 InverseSize;

flat in float layer;

void main() 
{
    vec3 fragCoord = vec3(gl_FragCoord.xy, layer);
    vec3 vL = texture(VelocityTexture, InverseSize * (fragCoord - vec3(1, 0, 0))).xyz;
    vec3 vR = texture(VelocityTexture, InverseSize * (fragCoord + vec3(1, 0, 0))).xyz;
    vec3 vB = texture(VelocityTexture, InverseSize * (fragCoord - vec3(0, 1, 0))).xyz;
    vec3 vT = texture(VelocityTexture, InverseSize * (fragCoord + vec3(0, 1, 0))).xyz;
    vec3 vU = texture(VelocityTexture, InverseSize * (fragCoord - vec3(0, 0, 1))).xyz;
    vec3 vD = texture(VelocityTexture, InverseSize * (fragCoord + vec3(0, 0, 1))).xyz;
    float obsL = texture(ObstacleTexture, InverseSize * (fragCoord - vec3(1, 0, 0))).x;
    float obsR = texture(ObstacleTexture, InverseSize * (fragCoord + vec3(1, 0, 0))).x;
    float obsB = texture(ObstacleTexture, InverseSize * (fragCoord - vec3(0, 1, 0))).x;
    float obsT = texture(ObstacleTexture, InverseSize * (fragCoord + vec3(0, 1, 0))).x;
    float obsU = texture(ObstacleTexture, InverseSize * (fragCoord - vec3(0, 0, 1))).x;
    float obsD = texture(ObstacleTexture, InverseSize * (fragCoord + vec3(0, 0, 1))).x;
    vec3 obsVelL = texture(ObstacleVelocityTexture, InverseSize * (fragCoord - vec3(1, 0, 0))).xyz;
    vec3 obsVelR = texture(ObstacleVelocityTexture, InverseSize * (fragCoord + vec3(1, 0, 0))).xyz;
    vec3 obsVelB = texture(ObstacleVelocityTexture, InverseSize * (fragCoord - vec3(0, 1, 0))).xyz;
    vec3 obsVelT = texture(ObstacleVelocityTexture, InverseSize * (fragCoord + vec3(0, 1, 0))).xyz;
    vec3 obsVelU = texture(ObstacleVelocityTexture, InverseSize * (fragCoord - vec3(0, 0, 1))).xyz;
    vec3 obsVelD = texture(ObstacleVelocityTexture, InverseSize * (fragCoord + vec3(0, 0, 1))).xyz;
    if (obsL > 0.0) vL = obsVelL;
    if (obsR > 0.0) vR = obsVelR;
    if (obsB > 0.0) vB = obsVelB;
    if (obsT > 0.0) vT = obsVelT;
    if (obsU > 0.0) vU = obsVelU;
    if (obsD > 0.0) vD = obsVelD;
    FragColor = 0.5 * (vR.x - vL.x + vT.y - vB.y + vD.z - vU.z);
    if (abs(FragColor) < 0.0001)
        FragColor = 0.0;
}


