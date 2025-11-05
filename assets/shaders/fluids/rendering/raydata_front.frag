/*
    OpenGL 4.1 Core - RayData - Fragment Shader (front faces)
*/

#version 410 core

out vec4 fragColor; // output color

uniform vec3 grid_size; // Number of voxels in each direction

uniform sampler2D RayDataTexture; // Ray data back texture
uniform sampler2D SceneDepthTexture; // Scene depth texture
uniform bool useSceneDepth = false; // optional scene depth usage

in vec3 texPos; // Position in texture space

uniform vec2 InverseSize; // Inverse of the size of the viewport

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
    // Get the ray data back value
    vec4 raydata = texture(RayDataTexture, InverseSize * gl_FragCoord.xy);

    // Get the depth of the scene
    float sceneDepth = texture(SceneDepthTexture, InverseSize * gl_FragCoord.xy).r;

    // Compute the position of the ray start in discrete texture space
    vec3 pos = TextureVoxelAlign(texPos);

    if (useSceneDepth) {
        // If the depth of the scene is less than the depth of the fragment,
        // set the position of the ray start as a negative value
        if (sceneDepth < gl_FragCoord.z)
            pos = vec3(-1.0);
    }

    // Set the position of the ray start in xyz component,
    // and the negate depth of the position in the raydata back 
    // in the w component
    fragColor = vec4(pos, raydata.w);
}


