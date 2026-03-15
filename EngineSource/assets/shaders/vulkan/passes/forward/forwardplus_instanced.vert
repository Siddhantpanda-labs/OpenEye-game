#version 460
// Resource-space: set 0 = Frame, set 4 binding 1 = instance transform buffer.

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(set = 0, binding = 0) uniform FrameBlock {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProj;
};

layout(set = 4, binding = 1) readonly buffer InstanceModels {
    mat4 u_InstanceModel[];
};

layout(push_constant, std430) uniform ForwardPlusPush {
    vec4 u_UVTransform;
    float u_UVRotation;
    float u_TexCoordIndex;
    float u_FlipV;
} pc;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_ViewPos;
layout(location = 2) out vec3 v_WorldNormal;
layout(location = 3) out vec2 v_TexCoord;
layout(location = 4) out mat3 v_TBN;

void main() {
    mat4 u_Model = u_InstanceModel[gl_InstanceIndex];
    vec4 w = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = w.xyz;
    v_ViewPos = (u_View * w).xyz;
    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    v_WorldNormal = normalize(normalMatrix * a_Normal);
    vec3 worldTangent = length(a_Tangent) > 0.001 ? normalize(normalMatrix * a_Tangent) : (abs(v_WorldNormal.y) < 0.999 ? normalize(cross(vec3(0, 1, 0), v_WorldNormal)) : normalize(cross(vec3(1, 0, 0), v_WorldNormal)));
    vec3 T = normalize(worldTangent - dot(worldTangent, v_WorldNormal) * v_WorldNormal);
    v_TBN = mat3(T, cross(v_WorldNormal, T), v_WorldNormal);
    vec2 uv = a_TexCoord * pc.u_UVTransform.zw;
    float c = cos(pc.u_UVRotation);
    float s = sin(pc.u_UVRotation);
    uv = mat2(c, -s, s, c) * uv + pc.u_UVTransform.xy;
    if (pc.u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    v_TexCoord = uv;
    gl_Position = u_Projection * u_View * w;
}
