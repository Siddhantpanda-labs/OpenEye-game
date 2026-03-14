#version 460

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

#define MAX_BONES 128
layout(std140, binding = 3) uniform Bones {
    mat4 u_Bones[MAX_BONES];
};

layout(push_constant, std430) uniform ForwardPush {
    mat4 u_Model;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_UVTransform;
    float u_UVRotation;
    float u_TexCoordIndex;
    float u_FlipV;
} pc;

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
    vec4 worldPos = pc.u_Model * pos;
    v_Position = worldPos.xyz;
    vec4 viewPos = pc.u_View * worldPos;
    mat3 normalMatrix = mat3(transpose(inverse(pc.u_Model)));
    vec3 worldNormal = normalize(normalMatrix * nrm);
    vec3 worldTangent = length(tangent) < 0.001
        ? (abs(worldNormal.y) < 0.999 ? normalize(cross(vec3(0.0, 1.0, 0.0), worldNormal)) : normalize(cross(vec3(1.0, 0.0, 0.0), worldNormal)))
        : normalize(normalMatrix * tangent);
    v_TBN = mat3(worldTangent, normalize(cross(worldNormal, worldTangent)), worldNormal);
    v_Normal = worldNormal;
    vec2 uv = a_TexCoord * pc.u_UVTransform.zw;
    float c = cos(pc.u_UVRotation);
    float s = sin(pc.u_UVRotation);
    uv = mat2(c, -s, s, c) * uv + pc.u_UVTransform.xy;
    if (pc.u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    v_TexCoord = uv;
    gl_Position = pc.u_Projection * viewPos;
}
