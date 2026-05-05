#version 460 core
// Resource-space: Frame UBO binding 0; model via uniform (OpenGL has no push constants).

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

uniform mat4 u_Model;

void main() {
    vec4 w = u_Model * vec4(a_Position, 1.0);
    v_Position = w.xyz;
    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    vec3 worldNormal = normalize(normalMatrix * a_Normal);
    vec3 worldTangent = length(a_Tangent) > 0.001
        ? normalize(normalMatrix * a_Tangent)
        : (abs(worldNormal.y) < 0.999 ? normalize(cross(vec3(0.0, 1.0, 0.0), worldNormal)) : normalize(cross(vec3(1.0, 0.0, 0.0), worldNormal)));
    vec3 T = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
    v_TBN = mat3(T, cross(worldNormal, T), worldNormal);
    v_Normal = worldNormal;
    v_TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
    gl_Position = u_Projection * u_View * w;
}
