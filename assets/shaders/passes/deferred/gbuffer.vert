#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;

uniform mat4 u_Model;
uniform bool u_IsInstanced = false;
layout(location = 5) in mat4 i_Model;

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
    mat3 normalMatrix = mat3(transpose(inverse(modelMatrix)));
    vec4 worldPos = modelMatrix * vec4(a_Position, 1.0);
    v_Position = worldPos.xyz;
    v_Normal = normalize(normalMatrix * a_Normal);
    // Match Vulkan engine: flip V to account for image origin conventions
    v_TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
    
    // Build TBN matrix for normal mapping
    vec3 T = normalize(normalMatrix * a_Tangent);
    vec3 N = normalize(normalMatrix * a_Normal);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt reorthogonalization
    vec3 B = normalize(normalMatrix * a_Bitangent);
    B = normalize(B - dot(B, N) * N - dot(B, T) * T);
    v_TBN = mat3(T, B, N);
    
    gl_Position = u_ProjectionMatrix * u_ViewMatrix * worldPos;
}


