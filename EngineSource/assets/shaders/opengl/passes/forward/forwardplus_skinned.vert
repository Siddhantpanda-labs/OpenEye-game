#version 460 core
// Frame UBO 0; SceneBuffers bone SSBO 1; model + UV uniforms.

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_ViewPos;
layout(location = 2) out vec3 v_WorldNormal;
layout(location = 3) out vec2 v_TexCoord;
layout(location = 4) out mat3 v_TBN;

layout(std140, binding = 0) uniform FrameBlock {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProj;
};

#define MAX_BONES 128
layout(std430, binding = 1) readonly buffer BonesBlock {
    mat4 u_Bones[MAX_BONES];
};

uniform mat4 u_Model;
uniform vec4 u_UVTransform;
uniform float u_UVRotation;
uniform float u_TexCoordIndex;
uniform float u_FlipV;

void main() {
    vec4 pos = vec4(a_Position, 1.0);
    vec3 nrm = a_Normal;
    vec3 tangent = a_Tangent;
    mat4 skin = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        int idx = a_BoneIndices[i];
        float w = a_BoneWeights[i];
        if (w > 0.0 && idx >= 0 && idx < MAX_BONES)
            skin += u_Bones[idx] * w;
    }
    pos = skin * pos;
    nrm = mat3(skin) * nrm;
    tangent = mat3(skin) * tangent;
    vec4 worldPos = u_Model * pos;
    v_WorldPos = worldPos.xyz;
    v_ViewPos = (u_View * worldPos).xyz;
    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    vec3 worldNormal = normalize(normalMatrix * nrm);
    vec3 worldTangent = length(tangent) < 0.001
        ? (abs(worldNormal.y) < 0.999 ? normalize(cross(vec3(0.0, 1.0, 0.0), worldNormal)) : normalize(cross(vec3(1.0, 0.0, 0.0), worldNormal)))
        : normalize(normalMatrix * tangent);
    v_TBN = mat3(worldTangent, normalize(cross(worldNormal, worldTangent)), worldNormal);
    v_WorldNormal = worldNormal;
    vec2 uv = a_TexCoord * u_UVTransform.zw;
    float c = cos(u_UVRotation);
    float s = sin(u_UVRotation);
    uv = mat2(c, -s, s, c) * uv + u_UVTransform.xy;
    if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    v_TexCoord = uv;
    gl_Position = u_Projection * u_View * worldPos;
}
