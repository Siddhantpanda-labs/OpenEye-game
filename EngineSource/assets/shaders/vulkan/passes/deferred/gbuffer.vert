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
layout(location = 1) out vec3 v_WorldNormal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out vec2 v_Velocity;

layout(push_constant, std430) uniform GBufferPush {
    mat4 u_Model;
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_PrevView;
    mat4 u_PrevProjection;
    vec4 u_UVTransform;
    float u_UVRotation;
    float u_TexCoordIndex;
    float u_FlipV;
} pc;

void main() {
    vec4 w = pc.u_Model * vec4(a_Position, 1.0);
    v_WorldPos = w.xyz;
    v_WorldNormal = mat3(transpose(inverse(pc.u_Model))) * a_Normal;
    vec2 baseUv = mix(a_TexCoord, a_TexCoord1, step(0.5, pc.u_TexCoordIndex));
    vec2 uv = baseUv * pc.u_UVTransform.zw;
    float c = cos(pc.u_UVRotation);
    float s = sin(pc.u_UVRotation);
    uv = mat2(c, -s, s, c) * uv;
    uv += pc.u_UVTransform.xy;
    if (pc.u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    v_TexCoord = uv;
    vec4 currentPos = pc.u_Projection * pc.u_View * w;
    vec4 prevPos = pc.u_PrevProjection * pc.u_PrevView * w;
    v_Velocity = (currentPos.xy / currentPos.w) - (prevPos.xy / prevPos.w);
    gl_Position = currentPos;
}
