#version 460
// Resource-space: set 0 = Frame, set 3 binding 1 = bones (or per-draw transform buffer when u_UsePerDrawTransformBuffer). Push: model, UV, flags.

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_WorldNormal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out vec2 v_Velocity;
layout(location = 4) out mat3 v_TBN;

#define MAX_BONES 128

layout(set = 0, binding = 0) uniform FrameBlock {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProj;
    mat4 u_PrevView;
    mat4 u_PrevProjection;
};

layout(set = 3, binding = 1) readonly buffer BoneOrTransformBlock {
    mat4 u_Data[];
};

/* Fragment uses float u_HasAlbedoTexture at offset 92 (shared with static gbuffer). VS-only int starts at 96. */
layout(push_constant, std430) uniform GBufferSkinnedPush {
    mat4 u_Model;
    vec4 u_UVTransform;
    float u_UVRotation;
    float u_TexCoordIndex;
    float u_FlipV;
    layout(offset = 96) int u_UsePerDrawTransformBuffer;
} pc;

void main() {
    vec4 pos = vec4(a_Position, 1.0);
    vec3 nrm = a_Normal;
    vec3 tang = a_Tangent;
    mat4 modelMat = pc.u_Model;

    if (pc.u_UsePerDrawTransformBuffer != 0) {
        modelMat = u_Data[gl_DrawID];
    } else {
        mat4 skin = mat4(0.0);
        float totalWeight = 0.0;
        for (int i = 0; i < 4; ++i) {
            int idx = a_BoneIndices[i];
            float w = a_BoneWeights[i];
            if (w > 0.0 && idx >= 0 && idx < MAX_BONES) {
                skin += u_Data[idx] * w;
                totalWeight += w;
            }
        }
        if (totalWeight <= 0.0) skin = mat4(1.0);
        pos = skin * pos;
        nrm = mat3(skin) * nrm;
        tang = mat3(skin) * tang;
    }

    vec4 worldPos = modelMat * pos;
    mat3 normalMatrix = mat3(transpose(inverse(modelMat)));
    vec3 worldNormal = normalize(normalMatrix * nrm);
    vec3 worldTangent = length(tang) < 0.001
        ? (abs(worldNormal.y) < 0.999 ? normalize(cross(vec3(0.0, 1.0, 0.0), worldNormal)) : normalize(cross(vec3(1.0, 0.0, 0.0), worldNormal)))
        : normalize(normalMatrix * tang);
    v_WorldPos = worldPos.xyz;
    v_WorldNormal = worldNormal;
    vec2 baseUv = mix(a_TexCoord, a_TexCoord1, step(0.5, pc.u_TexCoordIndex));
    vec2 uv = baseUv * pc.u_UVTransform.zw;
    float c = cos(pc.u_UVRotation);
    float s = sin(pc.u_UVRotation);
    uv = mat2(c, -s, s, c) * uv + pc.u_UVTransform.xy;
    if (pc.u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    v_TexCoord = uv;
    vec4 currentPos = u_Projection * u_View * worldPos;
    vec4 prevPos = u_PrevProjection * u_PrevView * worldPos;
    v_Velocity = (currentPos.xy / currentPos.w) - (prevPos.xy / prevPos.w);
    vec3 T = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
    v_TBN = mat3(T, cross(worldNormal, T), worldNormal);
    gl_Position = currentPos;
}
