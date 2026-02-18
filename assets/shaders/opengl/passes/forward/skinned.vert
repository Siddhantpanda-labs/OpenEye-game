#version 460 core

// Canonical mesh layout (all declared)
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec4 u_UVTransform; // xy = offset, zw = scale
uniform float u_UVRotation; // radians
uniform float u_TexCoordIndex; // 0 = UV0, 1 = UV1
uniform float u_FlipV; // 1 = flip V

#define MAX_BONES 128
layout(std140, binding = 3) uniform Bones {
    mat4 u_Bones[MAX_BONES];
};

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    vec4 pos = vec4(a_Position, 1.0);
    vec3 nrm = a_Normal;

    // According to advise.md: Skinning always happens. Identity skinning is valid skinning.
    // Remove branch - always compute skin matrix (Unity-style)
    mat4 skin = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        int idx = a_BoneIndices[i];
        float w = a_BoneWeights[i];
        if (w > 0.0 && idx >= 0 && idx < MAX_BONES) {
            skin += u_Bones[idx] * w;
        }
    }
    
    // For static meshes: bone = identity, weights = (1,0,0,0), result = unchanged
    // For animated meshes: bone = animated transform, result = skinned
    pos = skin * pos;
    nrm = mat3(skin) * nrm;

    vec4 worldPos = u_Model * pos;
    v_Position = worldPos.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * nrm;
    vec2 baseUv = mix(a_TexCoord, a_TexCoord1, step(0.5, u_TexCoordIndex));
    vec2 uv = baseUv * u_UVTransform.zw;
    float c = cos(u_UVRotation);
    float s = sin(u_UVRotation);
    uv = mat2(c, -s, s, c) * uv;
    uv += u_UVTransform.xy;
    if (u_FlipV > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    v_TexCoord = uv;
    gl_Position = u_Projection * u_View * worldPos;
}
