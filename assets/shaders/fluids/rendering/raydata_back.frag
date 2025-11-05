/*
    OpenGL 4.1 Core - RayData - Fragment Shader

    This shader is part of the program that creates the ray data texture, 
    which is used to render the fluid with the ray marching technique.
*/

#version 410 core

out vec4 fragColor; // output color

uniform vec3 grid_size; // Number of voxels in each dimension

uniform sampler2D SceneDepthTexture; // Scene depth texture
uniform bool useSceneDepth = false; // optional scene depth usage

uniform vec2 InverseSize; // Inverse of the size of the viewport

in vec3 texPos; // Position of the vertex in the texture space

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

// Apply the texture space alignment to the given position
// in order to obtain the coordinates in the discrete space
vec3 TextureVoxelAlign(vec3 pos)
{
    pos *= (grid_size - 1.0);
    pos += 0.5;
    pos /= grid_size;
    return pos;
}

// Main function
void main()
{
    // Compute the ray's end position in the texture space
    vec3 end = TextureVoxelAlign(texPos);

    // Read the scene depth
    float sceneDepth = texture(SceneDepthTexture, InverseSize * gl_FragCoord.xy).r;

    if (useSceneDepth) {
        // Check if some obstacle is in front of the fluid volume back face
        if (sceneDepth < gl_FragCoord.z)
        {
            vec3 scenePos = 2 * vec3(InverseSize * gl_FragCoord.xy, sceneDepth) - 1.0;
            vec4 localScenePos = inverse(projection * view) * vec4(scenePos, 1.0);
            localScenePos /= localScenePos.w;
            localScenePos = localScenePos * transpose(inverse(model));
            scenePos = localScenePos.xyz;
            scenePos = (scenePos + 1.0) / 2.0;
            scenePos.z = 1 - scenePos.z;
            end = TextureVoxelAlign(scenePos);
        }
    }
 
    // Store the end position and the negate depth of the position
    fragColor = vec4(end, - gl_FragCoord.z);
}


