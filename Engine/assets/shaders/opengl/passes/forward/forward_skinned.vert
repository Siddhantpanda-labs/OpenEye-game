#version 460 core
// Frame UBO 0; SceneBuffers bone SSBO 1; model + UV via uniforms.

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(location = 0) out vec3 v_Position;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out mat3 v_TBN;

layout(std140, binding = 0) uniform FrameBlock {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProj;
};

#define MAX_BONES 128
layout(std430, binding = 1) readonly buffer BoneBlock {
    mat4 u_Bones[MAX_BONES];
} boneData;

uniform mat4 u_Model;
uniform vec4 u_UVTransform = vec4(0.0, 0.0, 1.0, 1.0);
uniform float u_UVRotation = 0.0;
uniform float u_TexCoordIndex = 0.0;
uniform float u_FlipV = 0.0;

void main() {
    vec4 pos = vec4(a_Position, 1.0);
    vec3 nrm = a_Normal;
    vec3 tang = a_Tangent;

    mat4 skin = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        int idx = a_BoneIndices[i];
        float w = a_BoneWeights[i];
        if (w > 0.0 && idx >= 0 && idx < MAX_BONES) {
            skin += boneData.u_Bones[idx] * w;
        }
    }

    pos = skin * pos;
    nrm = mat3(skin) * nrm;
    tang = mat3(skin) * tang;

    vec4 worldPos = u_Model * pos;
    v_Position = worldPos.xyz;
    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    vec3 worldNormal = normalize(normalMatrix * nrm);
    vec3 worldTangent = length(tang) > 0.001
        ? normalize(normalMatrix * tang)
        : (abs(worldNormal.y) < 0.999 ? normalize(cross(vec3(0.0, 1.0, 0.0), worldNormal)) : normalize(cross(vec3(1.0, 0.0, 0.0), worldNormal)));
    vec3 T = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
    vec3 B = cross(worldNormal, T);
    v_TBN = mat3(T, B, worldNormal);
    v_Normal = worldNormal;
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
