#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;
layout(location = 5) in ivec4 a_BoneIndices;  // 4 bone indices per vertex
layout(location = 6) in vec4 a_BoneWeights;   // 4 bone weights per vertex

uniform mat4 u_Model;
uniform bool u_IsInstanced = false;
// Note: use distinct attribute locations for instanced model matrix to avoid
// colliding with bone attributes (5 and 6). We aren't instancing skinned meshes
// currently, but keeping this for future compatibility.
layout(location = 7) in mat4 i_Model;

// Bone matrices via UBO to avoid vertex uniform limits
#define MAX_BONES 512
layout(std140, binding = 3) uniform Bones {
    mat4 u_BoneMatrices[MAX_BONES];
};

#include "../../common/camera.glsl"

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;
out mat3 v_TBN; // Tangent-Bitangent-Normal matrix for normal mapping

void main() {
    mat4 modelMatrix = u_Model;
    if (u_IsInstanced) {
        modelMatrix = i_Model;
    }
    
    // Apply bone skinning (GPU skinning)
    vec4 skinnedPosition = vec4(a_Position, 1.0);
    vec3 skinnedNormal = a_Normal;
    vec3 skinnedTangent = a_Tangent;
    vec3 skinnedBitangent = a_Bitangent;
    
    // Only apply skinning if bone weights are non-zero
    if (dot(a_BoneWeights, vec4(1.0)) > 0.001) {
        // Initialize skinning matrices to zero (will accumulate)
        mat4 boneTransform = mat4(0.0);
        mat3 boneNormalTransform = mat3(0.0);
        float totalWeight = 0.0;
        
        // Accumulate bone influences (up to 4 bones per vertex)
        for (int i = 0; i < 4; i++) {
            if (a_BoneIndices[i] >= 0 && a_BoneIndices[i] < MAX_BONES && a_BoneWeights[i] > 0.0) {
                mat4 boneMatrix = u_BoneMatrices[a_BoneIndices[i]];
                
                // Check if bone matrix is valid (not all zeros, not NaN)
                // Check if any column has non-zero length
                bool isValid = (length(boneMatrix[0]) > 0.001 || length(boneMatrix[1]) > 0.001 || 
                               length(boneMatrix[2]) > 0.001 || length(boneMatrix[3]) > 0.001);
                
                if (isValid) {
                    boneTransform += boneMatrix * a_BoneWeights[i];
                    mat3 boneMatrix3x3 = mat3(boneMatrix);
                    boneNormalTransform += boneMatrix3x3 * a_BoneWeights[i];
                    totalWeight += a_BoneWeights[i];
                }
            }
        }
        
        // Only apply bone transformation if we accumulated valid bones
        // If totalWeight is too small, use identity (no transformation)
        if (totalWeight > 0.001) {
            skinnedPosition = boneTransform * vec4(a_Position, 1.0);
            skinnedNormal = boneNormalTransform * a_Normal;
            skinnedTangent = boneNormalTransform * a_Tangent;
            skinnedBitangent = boneNormalTransform * a_Bitangent;
        }
        // If totalWeight is 0, skinnedPosition/normal/tangent/bitangent remain unchanged (original values)
    }
    
    // Apply model matrix to skinned position
    mat3 normalMatrix = mat3(transpose(inverse(modelMatrix)));
    vec4 worldPos = modelMatrix * skinnedPosition;
    v_Position = worldPos.xyz;
    v_Normal = normalize(normalMatrix * skinnedNormal);
    // Match Vulkan engine: flip V to account for image origin conventions
    v_TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
    
    // Build TBN matrix for normal mapping
    vec3 T = normalize(normalMatrix * skinnedTangent);
    vec3 N = normalize(normalMatrix * skinnedNormal);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt reorthogonalization
    vec3 B = normalize(normalMatrix * skinnedBitangent);
    B = normalize(B - dot(B, N) * N - dot(B, T) * T);
    v_TBN = mat3(T, B, N);
    
    gl_Position = u_ProjectionMatrix * u_ViewMatrix * worldPos;
}

