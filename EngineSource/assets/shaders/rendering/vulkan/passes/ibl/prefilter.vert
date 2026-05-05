#version 460

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(location = 0) out vec3 v_WorldPos;

layout(push_constant, std430) uniform IBLPush {
    mat4 u_Projection;
    mat4 u_View;
} pc;

void main() {
    v_WorldPos = a_Position;
    mat4 rotView = mat4(mat3(pc.u_View));
    vec4 clipPos = pc.u_Projection * rotView * vec4(a_Position, 1.0);
    gl_Position = clipPos.xyww;
}
